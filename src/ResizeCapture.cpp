#include "ResizeCapture.h"
#include <sstream>

namespace DG
{
    /// Constructor that opens the video file with resize dimensions
    /// @param filename Path to the video file to open
    /// @param target_width Target width for resized frames
    /// @param target_height Target height for resized frames
    ResizeCapture::ResizeCapture(const char *filename, int target_width, int target_height)
        : m_target_width(target_width), m_target_height(target_height)
    {
        open(filename, target_width, target_height);
    }

    /// Destructor to clean up resources
    ResizeCapture::~ResizeCapture()
    {
        // Free filter graph
        if (m_filter_graph)
        {
            avfilter_graph_free(&m_filter_graph);
            m_filter_graph = nullptr;
            m_buffersrc_ctx = nullptr;
            m_buffersink_ctx = nullptr;
        }
        // Base class destructor will handle the rest
    }

    /// Open a video file for reading with target resize dimensions
    /// @param filename Path to the video file to open
    /// @param target_width Target width for resized frames
    /// @param target_height Target height for resized frames
    /// @return True if the video was successfully opened, false otherwise
    bool ResizeCapture::open(const char *filename, int target_width, int target_height)
    {
        m_target_width = target_width;
        m_target_height = target_height;

        // Call base class open first, then initialize filter graph if successful
        if (VideoCapture::open(filename))
        {
            if (initFilterGraph())
            {
                return true;
            }
        }
        return false;
    }

    /// Read the next video frame and process it through the filter graph
    /// @param dst_frame Pointer to an AVFrame for BGR24 output
    /// @return true on success, false on EOS or error
    bool ResizeCapture::readFrame(AVFrame *dst_frame)
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

    bool ResizeCapture::initFilterGraph()
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
