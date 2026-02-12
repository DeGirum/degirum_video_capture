#include "VideoCapture.h"
#include "opencv_enums.h"
#include <sstream>

namespace DG
{
    /// Constructor that opens the video file immediately
    /// @param filename Path to the video file to open
    VideoCapture::VideoCapture(const char *filename)
    {
        open(filename, 0, 0);
    }

    /// Constructor that opens the video file with resize dimensions
    /// @param filename Path to the video file to open
    /// @param target_width Target width for resized frames
    /// @param target_height Target height for resized frames
    VideoCapture::VideoCapture(const char *filename, int target_width, int target_height)
        : m_target_width(target_width), m_target_height(target_height)
    {
        open(filename, target_width, target_height);
    }

    /// Destructor to clean up resources
    VideoCapture::~VideoCapture()
    {
        close();

        // Free filter graph if exists
        if (m_filter_graph)
        {
            avfilter_graph_free(&m_filter_graph);
            m_filter_graph = nullptr;
            m_buffersrc_ctx = nullptr;
            m_buffersink_ctx = nullptr;
        }
    }

    /// Open a video file for reading with target resize dimensions
    /// @param filename Path to the video file to open
    /// @param target_width Target width for resized frames
    /// @param target_height Target height for resized frames
    /// @return True if the video was successfully opened, false otherwise
    bool VideoCapture::open(const char *filename, int target_width, int target_height)
    {
        // Clean up any existing resources if already opened
        close();

        m_target_width = target_width;
        m_target_height = target_height;

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

        // Choose implementation based on resize needed
        if (m_target_width > 0 && m_target_height > 0)
        {
            m_readFrameImpl = &VideoCapture::readFrameFiltered;

            // Initialize filter graph for resizing and format conversion
            if (!initFilterGraph())
            {
                // Filter graph init failed, clean up and return false
                close();
                return false;
            }
        }
        else
        {
            m_readFrameImpl = &VideoCapture::readFrameDirect;
        }

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

        // Free filter graph if exists
        if (m_filter_graph)
        {
            avfilter_graph_free(&m_filter_graph);
            m_filter_graph = nullptr;
            m_buffersrc_ctx = nullptr;
            m_buffersink_ctx = nullptr;
        }

        // Reset properties
        m_video_stream_index = -1;
        m_width = m_height = 0;
        m_src_pix_fmt = AV_PIX_FMT_NONE;
        m_flush_pending = false;
        m_frame_count = 0;
        m_last_pts = AV_NOPTS_VALUE;
        m_target_width = 0;
        m_target_height = 0;
    }

    /// Check if the video file is currently opened
    /// @return True if the video is opened, false otherwise
    bool VideoCapture::isOpened() const
    {
        return m_fmt_ctx && m_codec_ctx;
    }

    /// Get video capture property value (OpenCV-compatible)
    /// @param propId Property identifier from cv::VideoCaptureProperties enum
    /// @return Property value, or -1 if property is not supported/available
    double VideoCapture::get(int propId) const
    {
        if (!isOpened())
            return -1;

        switch (propId)
        {
        case cv::CAP_PROP_FRAME_WIDTH:
            return static_cast<double>(m_width);

        case cv::CAP_PROP_FRAME_HEIGHT:
            return static_cast<double>(m_height);

        case cv::CAP_PROP_FPS:
        {
            AVStream *stream = m_fmt_ctx->streams[m_video_stream_index];
            if (stream->avg_frame_rate.den > 0)
                return av_q2d(stream->avg_frame_rate);
            else if (stream->r_frame_rate.den > 0)
                return av_q2d(stream->r_frame_rate);
            return -1;
        }

        case cv::CAP_PROP_FRAME_COUNT:
        {
            AVStream *stream = m_fmt_ctx->streams[m_video_stream_index];
            if (stream->nb_frames > 0)
                return static_cast<double>(stream->nb_frames);
            // Try to estimate from duration and fps
            if (stream->duration != AV_NOPTS_VALUE && stream->avg_frame_rate.den > 0)
            {
                double duration_sec = stream->duration * av_q2d(stream->time_base);
                double fps = av_q2d(stream->avg_frame_rate);
                return duration_sec * fps;
            }
            return -1;
        }

        case cv::CAP_PROP_POS_FRAMES:
            return static_cast<double>(m_frame_count);

        case cv::CAP_PROP_POS_MSEC:
        {
            if (m_last_pts == AV_NOPTS_VALUE)
                return 0;
            AVStream *stream = m_fmt_ctx->streams[m_video_stream_index];
            double time_sec = m_last_pts * av_q2d(stream->time_base);
            return time_sec * 1000.0; // Convert to milliseconds
        }

        case cv::CAP_PROP_POS_AVI_RATIO:
        {
            double total = get(cv::CAP_PROP_FRAME_COUNT);
            if (total > 0)
                return m_frame_count / total;
            return 0;
        }

        case cv::CAP_PROP_FOURCC:
            return static_cast<double>(m_codec_ctx->codec_tag);

        default:
            return -1;
        }
    }

    /// Call readFrameDirect or readFrameFiltered based on if resizing+padding is enabled
    /// @param dst Pointer to an AVFrame for output (must be pre-allocated and configured by caller)
    /// @return true on success, false on EOS or error
    bool VideoCapture::readFrame(AVFrame *dst)
    {
        // Single indirection, no branch
        return (this->*m_readFrameImpl)(dst);
    }

    /// Read the next video frame, convert it to BGR24 format, and store it in the provided AVFrame
    /// @param dst_frame Pointer to an AVFrame that is already allocated and configured for BGR24 output
    /// @note dst_frame must be:
    /// @note   - allocated with av_frame_alloc()
    /// @note   - format = AV_PIX_FMT_BGR24
    /// @note   - av_frame_get_buffer(dst_frame, 32) already called
    /// @return true on success (dst_frame filled with BGR24), false on EOS or error.
    bool VideoCapture::readFrameDirect(AVFrame *dst_frame)
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
            // Update position tracking
            m_frame_count++;
            m_last_pts = m_yuv_frame->pts;

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

    /// Read the next video frame and process it through the filter graph
    /// @param dst_frame Pointer to an AVFrame for BGR24 output
    /// @return true on success, false on EOS or error
    bool VideoCapture::readFrameFiltered(AVFrame *dst_frame)
    {
        // Check if video is opened and filter graph is initialized
        if (!isOpened() || !m_filter_graph || !dst_frame)
            return false;

        // Allocate packet for reading encoded data
        AVPacket *pkt = av_packet_alloc();
        if (!pkt)
            return false;

        bool result = false;

        // Loop until we get a processed frame or reach end of file
        for (;;)
        {
            // Read next packet from the video stream
            int ret = av_read_frame(m_fmt_ctx, pkt);

            // If we hit end of file, flush the decoder and filter graph
            if (ret < 0)
            {
                if (!m_flush_pending)
                {
                    m_flush_pending = true;
                    // Send null packet to decoder to signal end of stream
                    avcodec_send_packet(m_codec_ctx, nullptr);
                }

                // Try to receive any remaining frames from decoder
                ret = avcodec_receive_frame(m_codec_ctx, m_yuv_frame);
                if (ret == 0)
                {
                    // Push to filter graph
                    if (av_buffersrc_add_frame_flags(m_buffersrc_ctx, m_yuv_frame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0)
                    {
                        // Try to pull from filter graph
                        ret = av_buffersink_get_frame(m_buffersink_ctx, dst_frame);
                        if (ret >= 0)
                        {
                            result = true;
                            break;
                        }
                    }
                }
                else
                {
                    // Flush filter graph
                    if (av_buffersrc_add_frame_flags(m_buffersrc_ctx, nullptr, 0) >= 0)
                    {
                        ret = av_buffersink_get_frame(m_buffersink_ctx, dst_frame);
                        if (ret >= 0)
                        {
                            result = true;
                        }
                    }
                    break;
                }
            }

            // Filter packets by stream (only process video stream)
            if (pkt->stream_index == m_video_stream_index)
            {
                // Send packet to decoder
                ret = avcodec_send_packet(m_codec_ctx, pkt);
                av_packet_unref(pkt);

                if (ret < 0)
                {
                    result = false;
                    break;
                }

                // Try to receive decoded frame
                ret = avcodec_receive_frame(m_codec_ctx, m_yuv_frame);
                if (ret == 0)
                {
                    // Push decoded YUV frame to filter graph
                    if (av_buffersrc_add_frame_flags(m_buffersrc_ctx, m_yuv_frame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0)
                    {
                        // Pull processed BGR frame from filter graph
                        ret = av_buffersink_get_frame(m_buffersink_ctx, dst_frame);
                        if (ret >= 0)
                        {
                            result = true;
                            break;
                        }
                        // EAGAIN means need more frames, continue loop
                    }
                }
                // EAGAIN means need more packets, continue loop
            }
            else
            {
                // Not a video packet, ignore it
                av_packet_unref(pkt);
            }
        }

        av_packet_free(&pkt);
        return result;
    }

    /// Create and initialize the FFmpeg filter graph for resizing and format conversion
    /// @return true on successful initialization, false on failure
    bool VideoCapture::initFilterGraph()
    {
        // Allocate filter graph
        m_filter_graph = avfilter_graph_alloc();
        if (!m_filter_graph)
        {
            return false;
        }

        // Create buffer source
        const AVFilter *buffersrc = avfilter_get_by_name("buffer");
        if (!buffersrc)
        {
            return false;
        }

        // Build source args
        std::ostringstream args;
        args << "video_size=" << m_codec_ctx->width << "x" << m_codec_ctx->height
             << ":pix_fmt=" << m_codec_ctx->pix_fmt
             << ":time_base=" << m_fmt_ctx->streams[m_video_stream_index]->time_base.num << "/" << m_fmt_ctx->streams[m_video_stream_index]->time_base.den
             << ":pixel_aspect=" << m_codec_ctx->sample_aspect_ratio.num << "/"
             << (m_codec_ctx->sample_aspect_ratio.den ? m_codec_ctx->sample_aspect_ratio.den : 1);

        // Setup buffer source with properties string
        if (avfilter_graph_create_filter(&m_buffersrc_ctx, buffersrc, "in",
                                         args.str().c_str(), nullptr, m_filter_graph) < 0)
        {
            return false;
        }

        // Create buffer sink
        const AVFilter *buffersink = avfilter_get_by_name("buffersink");
        if (!buffersink)
        {
            return false;
        }

        // Setup buffer sink
        if (avfilter_graph_create_filter(&m_buffersink_ctx, buffersink, "out",
                                         nullptr, nullptr, m_filter_graph) < 0)
        {
            return false;
        }

        // Set output pixel format to BGR24
        enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_BGR24, AV_PIX_FMT_NONE};
        if (av_opt_set_int_list(m_buffersink_ctx, "pix_fmts", pix_fmts,
                                AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0)
        {
            return false;
        }

        // Create filter chain: buffer -> scale -> pad -> format -> buffersink
        AVFilterContext *last_filter = m_buffersrc_ctx;

        // Scale filter: scale to target size maintaining aspect ratio
        std::ostringstream scale_args;
        scale_args << m_target_width << ":" << m_target_height
                   << ":force_original_aspect_ratio=decrease";

        // Create and setup scale filter
        const AVFilter *scale_filter = avfilter_get_by_name("scale");
        AVFilterContext *scale_ctx = nullptr;
        if (avfilter_graph_create_filter(&scale_ctx, scale_filter, "scale",
                                         scale_args.str().c_str(), nullptr, m_filter_graph) < 0)
        {
            return false;
        }

        // Link scale filter to buffer source
        if (avfilter_link(last_filter, 0, scale_ctx, 0) < 0)
        {
            return false;
        }
        last_filter = scale_ctx;

        // Pad filter: pad to exact target size, centered
        std::ostringstream pad_args;
        pad_args << m_target_width << ":" << m_target_height
                 << ":(ow-iw)/2:(oh-ih)/2";

        // Create and setup pad filter
        const AVFilter *pad_filter = avfilter_get_by_name("pad");
        AVFilterContext *pad_ctx = nullptr;
        if (avfilter_graph_create_filter(&pad_ctx, pad_filter, "pad",
                                         pad_args.str().c_str(), nullptr, m_filter_graph) < 0)
        {
            return false;
        }

        // Link pad filter to scale filter
        if (avfilter_link(last_filter, 0, pad_ctx, 0) < 0)
        {
            return false;
        }
        last_filter = pad_ctx;

        // Format filter: convert to BGR24
        const AVFilter *format_filter = avfilter_get_by_name("format");
        AVFilterContext *format_ctx = nullptr;
        if (avfilter_graph_create_filter(&format_ctx, format_filter, "format",
                                         "pix_fmts=bgr24", nullptr, m_filter_graph) < 0)
        {
            return false;
        }

        // Link format filter to pad filter
        if (avfilter_link(last_filter, 0, format_ctx, 0) < 0)
        {
            return false;
        }
        last_filter = format_ctx;

        // Link buffersink to total combined filter chain
        if (avfilter_link(last_filter, 0, m_buffersink_ctx, 0) < 0)
        {
            return false;
        }

        // Configure the graph
        if (avfilter_graph_config(m_filter_graph, nullptr) < 0)
        {
            return false;
        }

        return true;
    }

} // namespace DG
