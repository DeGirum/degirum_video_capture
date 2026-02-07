#include "VideoCapture.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace DG
{
    /// Constructor that opens the video file immediately
    /// @param filename Path to the video file to open
    VideoCapture::VideoCapture(const char *filename)
    {
        open(filename);
    }

    /// Destructor to clean up resources
    VideoCapture::~VideoCapture()
    {
        close();
    }

    /// Open a video file for reading
    /// @param filename Path to the video file to open
    /// @return True if the video was successfully opened, false otherwise
    bool VideoCapture::open(const char *filename)
    {
        // Clean up any existing resources if already opened
        close();

        // Open input stream and read header
        if (avformat_open_input(&m_fmt_ctx, filename, nullptr, nullptr) < 0)
            return false;

        // Get stream info (required for some formats/codecs to initialize properly)
        if (avformat_find_stream_info(m_fmt_ctx, nullptr) < 0)
            return false;

        // Find best video stream
        const AVCodec *decoder = nullptr;
        m_video_stream_index = av_find_best_stream(m_fmt_ctx, AVMEDIA_TYPE_VIDEO,
                                                   -1, -1, &decoder, 0);
        if (m_video_stream_index < 0)
            return false;

        // AVStream *video_stream = m_fmt_ctx->streams[m_video_stream_index];

        // Create codec context
        m_codec_ctx = avcodec_alloc_context3(decoder);
        if (!m_codec_ctx)
            return false;

        // Fill codec context from stream codec parameters
        if (avcodec_parameters_to_context(m_codec_ctx, m_fmt_ctx->streams[m_video_stream_index]->codecpar) < 0)
            return false;

        // Enable multi-threaded decoding (0 = auto-detect CPU cores)
        m_codec_ctx->thread_count = 0;
        m_codec_ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

        // Initialize codec context to use selected codec
        if (avcodec_open2(m_codec_ctx, decoder, nullptr) < 0)
            return false;

        // Store video properties for potential user retrieval
        m_width = m_codec_ctx->width;
        m_height = m_codec_ctx->height;
        m_src_pix_fmt = m_codec_ctx->pix_fmt;

        // Allocate internal decoded YUV frame (does not allocate buffers yet)
        m_yuv_frame = av_frame_alloc();
        if (!m_yuv_frame)
            return false;

        // Swscale context for YUV -> BGR24 conversion
        m_sws_ctx = sws_getContext(
            m_width, m_height, m_src_pix_fmt,
            m_width, m_height, AV_PIX_FMT_BGR24,
            SWS_BILINEAR,
            nullptr, nullptr, nullptr);
        if (!m_sws_ctx)
            return false;

        return true;
    }

    /// Close the video file and clean up all resources
    void VideoCapture::close()
    {
        // Flush and clean up decoder context first
        if (m_codec_ctx)
        {
            avcodec_flush_buffers(m_codec_ctx);
            avcodec_free_context(&m_codec_ctx);
            m_codec_ctx = nullptr;
        }

        // Free swscale context
        if (m_sws_ctx)
        {
            sws_freeContext(m_sws_ctx);
            m_sws_ctx = nullptr;
        }

        // Close input format context
        if (m_fmt_ctx)
        {
            avformat_close_input(&m_fmt_ctx);
            m_fmt_ctx = nullptr;
        }

        // Free internal YUV frame
        if (m_yuv_frame)
        {
            av_frame_unref(m_yuv_frame);
            av_frame_free(&m_yuv_frame);
            m_yuv_frame = nullptr;
        }

        // Reset properties
        m_video_stream_index = -1;
        m_width = m_height = 0;
        m_src_pix_fmt = AV_PIX_FMT_NONE;
        m_flush_pending = false;
    }

    /// Check if the video file is currently opened
    /// @return True if the video is opened, false otherwise
    bool VideoCapture::isOpened() const
    {
        return m_fmt_ctx && m_codec_ctx;
    }

    /// Get the video width in pixels
    /// @return Video width in pixels
    int VideoCapture::width() const { return m_width; }

    /// Get the video height in pixels
    /// @return Video height in pixels
    int VideoCapture::height() const { return m_height; }

    /// Get the source pixel format of the video frames
    /// @return AVPixelFormat enum value representing the source pixel format
    AVPixelFormat VideoCapture::srcPixelFormat() const { return m_src_pix_fmt; }

    /// Read the next video frame, convert it to BGR24 format, and store it in the provided AVFrame
    /// @param dst_frame Pointer to an AVFrame that is already allocated and configured for BGR24 output
    /// @note dst_frame must be:
    /// @note   - allocated with av_frame_alloc()
    /// @note   - format = AV_PIX_FMT_BGR24
    /// @note   - width/height set to this->width()/height()
    /// @note   - av_frame_get_buffer(dst_frame, 32) already called
    /// @return true on success (dst_frame filled with BGR24), false on EOS or error.
    bool VideoCapture::readFrame(AVFrame *dst_frame)
    {
        // Check if video is opened and dst_frame frame is valid
        if (!isOpened() || !dst_frame)
            return false;

        // Allocate packet for reading encoded data
        AVPacket *pkt = av_packet_alloc();
        if (!pkt)
            return false;

        bool result = false;

        // Loop until we get a frame or reach end of file
        for (;;)
        {
            // Read next packet from the video stream
            int ret = av_read_frame(m_fmt_ctx, pkt);

            // If we hit end of file, flush the decoder to get any remaining frames
            if (ret < 0)
            {
                if (!m_flush_pending)
                {
                    m_flush_pending = true;

                    // Send packet to decoder with null data to signal end of stream
                    avcodec_send_packet(m_codec_ctx, nullptr);
                }

                // Try to receive any remaining frames from decoder after flushing
                result = receiveAndConvert(dst_frame);
                break;
            }

            // Filter packets by stream (only process video stream, discard all others)
            if (pkt->stream_index == m_video_stream_index)
            {
                // Send packet to decoder
                ret = avcodec_send_packet(m_codec_ctx, pkt);

                // Free packet's internal buffers (packet is reused in the loop)
                av_packet_unref(pkt);
                if (ret < 0)
                {
                    result = false;
                    break;
                }

                // Check if yuv->brg converter has a complete and ready frame to output
                if (receiveAndConvert(dst_frame))
                {
                    result = true;
                    break;
                }
                // else: need more packets, continue
            }
            // Packet does not belong to video stream, ignore it and read next
            else
            {
                // Free packet's internal buffers since we're not using it
                av_packet_unref(pkt);
            }
        }

        // Free packet memory
        av_packet_free(&pkt);

        return result;
    }

    /// Internal helper to receive a decoded frame and convert it to BGR24 format
    /// @param dst_frame Pointer to an AVFrame that is already allocated and configured for BGR24 output
    /// @return true if a frame was received and converted, false if no frame is available (EAGAIN) or end of stream (EOF)
    bool VideoCapture::receiveAndConvert(AVFrame *dst_frame)
    {
        // Poll decoder for a decoded frame
        int ret = avcodec_receive_frame(m_codec_ctx, m_yuv_frame);

        // If we got a frame, convert it to BGR24 and return true
        if (ret == 0)
        {
            // Convert YUV -> BGR24 into caller's buffer (no extra copy afterward)
            sws_scale(
                m_sws_ctx,
                m_yuv_frame->data,
                m_yuv_frame->linesize,
                0,
                m_height,
                dst_frame->data,
                dst_frame->linesize);
            // Copy basic timing info if you care about PTS, etc.
            dst_frame->pts = m_yuv_frame->pts;
            return true;
        }
        // EAGAIN or EOF: no frame right now
        return false;
    }

} // namespace DG
