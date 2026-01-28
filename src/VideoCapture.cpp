//
// VideoCapture C++ Implementation
//
// Copyright 2026 DeGirum Corporation
//

#include "VideoCapture.h"
#include <stdexcept>
#include <sstream>

namespace degirum
{

    VideoCapture::VideoCapture()
        : format_ctx_(nullptr), codec_ctx_(nullptr), filter_graph_(nullptr), buffersrc_ctx_(nullptr), buffersink_ctx_(nullptr), video_stream_(nullptr), packet_(nullptr), av_frame_(nullptr), filtered_frame_(nullptr), is_opened_(false), video_stream_index_(-1), target_width_(0), target_height_(0), video_width_(0), video_height_(0), frame_number_(0)
    {
    }

    VideoCapture::~VideoCapture()
    {
        close();
    }

    void VideoCapture::cleanup()
    {
        if (filtered_frame_)
        {
            av_frame_free(&filtered_frame_);
            filtered_frame_ = nullptr;
        }

        if (av_frame_)
        {
            av_frame_free(&av_frame_);
            av_frame_ = nullptr;
        }

        if (packet_)
        {
            av_packet_free(&packet_);
            packet_ = nullptr;
        }

        if (filter_graph_)
        {
            avfilter_graph_free(&filter_graph_);
            filter_graph_ = nullptr;
            buffersrc_ctx_ = nullptr;
            buffersink_ctx_ = nullptr;
        }

        if (codec_ctx_)
        {
            avcodec_free_context(&codec_ctx_);
            codec_ctx_ = nullptr;
        }

        if (format_ctx_)
        {
            avformat_close_input(&format_ctx_);
            format_ctx_ = nullptr;
        }

        video_stream_ = nullptr;
        is_opened_ = false;
        video_stream_index_ = -1;
        frame_number_ = 0;
    }

    bool VideoCapture::open(const std::string &filename, int width, int height)
    {
        close();

        target_width_ = width;
        target_height_ = height;

        // Open input file
        if (avformat_open_input(&format_ctx_, filename.c_str(), nullptr, nullptr) < 0)
        {
            return false;
        }

        // Retrieve stream information
        if (avformat_find_stream_info(format_ctx_, nullptr) < 0)
        {
            cleanup();
            return false;
        }

        // Find the first video stream
        video_stream_index_ = -1;
        for (unsigned int i = 0; i < format_ctx_->nb_streams; i++)
        {
            if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                video_stream_index_ = i;
                video_stream_ = format_ctx_->streams[i];
                break;
            }
        }

        if (video_stream_index_ == -1)
        {
            cleanup();
            return false;
        }

        // Find decoder for the video stream
        const AVCodec *codec = avcodec_find_decoder(video_stream_->codecpar->codec_id);
        if (!codec)
        {
            cleanup();
            return false;
        }

        // Allocate codec context
        codec_ctx_ = avcodec_alloc_context3(codec);
        if (!codec_ctx_)
        {
            cleanup();
            return false;
        }

        // Copy codec parameters to codec context
        if (avcodec_parameters_to_context(codec_ctx_, video_stream_->codecpar) < 0)
        {
            cleanup();
            return false;
        }

        // Enable multi-threaded decoding
        codec_ctx_->thread_count = 0; // Auto-detect number of threads
        codec_ctx_->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

        // Open codec
        if (avcodec_open2(codec_ctx_, codec, nullptr) < 0)
        {
            cleanup();
            return false;
        }

        video_width_ = codec_ctx_->width;
        video_height_ = codec_ctx_->height;

        // Initialize filter graph
        if (!initFilterGraph())
        {
            cleanup();
            return false;
        }

        // Allocate reusable packet and frames
        packet_ = av_packet_alloc();
        av_frame_ = av_frame_alloc();
        filtered_frame_ = av_frame_alloc();

        if (!packet_ || !av_frame_ || !filtered_frame_)
        {
            cleanup();
            return false;
        }

        is_opened_ = true;
        frame_number_ = 0;

        return true;
    }

    bool VideoCapture::initFilterGraph()
    {
        filter_graph_ = avfilter_graph_alloc();
        if (!filter_graph_)
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
        args << "video_size=" << codec_ctx_->width << "x" << codec_ctx_->height
             << ":pix_fmt=" << codec_ctx_->pix_fmt
             << ":time_base=" << video_stream_->time_base.num << "/" << video_stream_->time_base.den
             << ":pixel_aspect=" << codec_ctx_->sample_aspect_ratio.num << "/"
             << (codec_ctx_->sample_aspect_ratio.den ? codec_ctx_->sample_aspect_ratio.den : 1);

        if (avfilter_graph_create_filter(&buffersrc_ctx_, buffersrc, "in",
                                         args.str().c_str(), nullptr, filter_graph_) < 0)
        {
            return false;
        }

        // Create buffer sink
        const AVFilter *buffersink = avfilter_get_by_name("buffersink");
        if (!buffersink)
        {
            return false;
        }

        if (avfilter_graph_create_filter(&buffersink_ctx_, buffersink, "out",
                                         nullptr, nullptr, filter_graph_) < 0)
        {
            return false;
        }

        // Set output pixel format to BGR24
        enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_BGR24, AV_PIX_FMT_NONE};
        if (av_opt_set_int_list(buffersink_ctx_, "pix_fmts", pix_fmts,
                                AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0)
        {
            return false;
        }

        // Create filter chain: buffer -> scale -> pad -> format -> buffersink
        AVFilterContext *last_filter = buffersrc_ctx_;

        // Scale filter: scale to target size maintaining aspect ratio
        std::ostringstream scale_args;
        scale_args << target_width_ << ":" << target_height_
                   << ":force_original_aspect_ratio=decrease";

        const AVFilter *scale_filter = avfilter_get_by_name("scale");
        AVFilterContext *scale_ctx = nullptr;
        if (avfilter_graph_create_filter(&scale_ctx, scale_filter, "scale",
                                         scale_args.str().c_str(), nullptr, filter_graph_) < 0)
        {
            return false;
        }

        if (avfilter_link(last_filter, 0, scale_ctx, 0) < 0)
        {
            return false;
        }
        last_filter = scale_ctx;

        // Pad filter: pad to exact target size, centered
        std::ostringstream pad_args;
        pad_args << target_width_ << ":" << target_height_
                 << ":(ow-iw)/2:(oh-ih)/2";

        const AVFilter *pad_filter = avfilter_get_by_name("pad");
        AVFilterContext *pad_ctx = nullptr;
        if (avfilter_graph_create_filter(&pad_ctx, pad_filter, "pad",
                                         pad_args.str().c_str(), nullptr, filter_graph_) < 0)
        {
            return false;
        }

        if (avfilter_link(last_filter, 0, pad_ctx, 0) < 0)
        {
            return false;
        }
        last_filter = pad_ctx;

        // Format filter: convert to BGR24
        const AVFilter *format_filter = avfilter_get_by_name("format");
        AVFilterContext *format_ctx = nullptr;
        if (avfilter_graph_create_filter(&format_ctx, format_filter, "format",
                                         "pix_fmts=bgr24", nullptr, filter_graph_) < 0)
        {
            return false;
        }

        if (avfilter_link(last_filter, 0, format_ctx, 0) < 0)
        {
            return false;
        }
        last_filter = format_ctx;

        // Link to sink
        if (avfilter_link(last_filter, 0, buffersink_ctx_, 0) < 0)
        {
            return false;
        }

        // Configure the graph
        if (avfilter_graph_config(filter_graph_, nullptr) < 0)
        {
            return false;
        }

        return true;
    }

    bool VideoCapture::read(VideoFrame &frame)
    {
        if (!is_opened_)
        {
            return false;
        }

        // Try to pull a filtered frame first (from filter graph buffer)
        if (av_buffersink_get_frame(buffersink_ctx_, filtered_frame_) >= 0)
        {
            // Copy frame data to output
            frame.width = filtered_frame_->width;
            frame.height = filtered_frame_->height;
            frame.channels = 3;

            int data_size = frame.width * frame.height * frame.channels;
            frame.data.resize(data_size);

            // Copy line by line (handle different line sizes/padding)
            for (int y = 0; y < frame.height; y++)
            {
                memcpy(frame.data.data() + y * frame.width * frame.channels,
                       filtered_frame_->data[0] + y * filtered_frame_->linesize[0],
                       frame.width * frame.channels);
            }

            av_frame_unref(filtered_frame_);
            frame_number_++;
            return true;
        }

        // No buffered frames, read more packets
        while (av_read_frame(format_ctx_, packet_) >= 0)
        {
            if (packet_->stream_index == video_stream_index_)
            {
                // Send packet to decoder
                if (avcodec_send_packet(codec_ctx_, packet_) >= 0)
                {
                    // Receive decoded frame
                    while (avcodec_receive_frame(codec_ctx_, av_frame_) >= 0)
                    {
                        // Push frame to filter graph
                        if (av_buffersrc_add_frame_flags(buffersrc_ctx_, av_frame_,
                                                         AV_BUFFERSRC_FLAG_KEEP_REF) >= 0)
                        {
                            // Try to pull a filtered frame
                            if (av_buffersink_get_frame(buffersink_ctx_, filtered_frame_) >= 0)
                            {
                                // Copy frame data to output
                                frame.width = filtered_frame_->width;
                                frame.height = filtered_frame_->height;
                                frame.channels = 3;

                                int data_size = frame.width * frame.height * frame.channels;
                                frame.data.resize(data_size);

                                // Copy line by line (handle different line sizes/padding)
                                for (int y = 0; y < frame.height; y++)
                                {
                                    memcpy(frame.data.data() + y * frame.width * frame.channels,
                                           filtered_frame_->data[0] + y * filtered_frame_->linesize[0],
                                           frame.width * frame.channels);
                                }

                                av_frame_unref(filtered_frame_);
                                av_frame_unref(av_frame_);
                                av_packet_unref(packet_);
                                frame_number_++;
                                return true;
                            }
                        }
                        av_frame_unref(av_frame_);
                    }
                }
            }
            av_packet_unref(packet_);
        }

        // End of file reached, flush the decoder
        avcodec_send_packet(codec_ctx_, nullptr);
        while (avcodec_receive_frame(codec_ctx_, av_frame_) >= 0)
        {
            // Push frame to filter graph
            if (av_buffersrc_add_frame_flags(buffersrc_ctx_, av_frame_,
                                             AV_BUFFERSRC_FLAG_KEEP_REF) >= 0)
            {
                // Try to pull a filtered frame
                if (av_buffersink_get_frame(buffersink_ctx_, filtered_frame_) >= 0)
                {
                    // Copy frame data to output
                    frame.width = filtered_frame_->width;
                    frame.height = filtered_frame_->height;
                    frame.channels = 3;

                    int data_size = frame.width * frame.height * frame.channels;
                    frame.data.resize(data_size);

                    // Copy line by line (handle different line sizes/padding)
                    for (int y = 0; y < frame.height; y++)
                    {
                        memcpy(frame.data.data() + y * frame.width * frame.channels,
                               filtered_frame_->data[0] + y * filtered_frame_->linesize[0],
                               frame.width * frame.channels);
                    }

                    av_frame_unref(filtered_frame_);
                    av_frame_unref(av_frame_);
                    frame_number_++;
                    return true;
                }
            }
            av_frame_unref(av_frame_);
        }

        // Flush the filter graph
        if (av_buffersrc_add_frame_flags(buffersrc_ctx_, nullptr, 0) >= 0)
        {
            if (av_buffersink_get_frame(buffersink_ctx_, filtered_frame_) >= 0)
            {
                // Copy frame data to output
                frame.width = filtered_frame_->width;
                frame.height = filtered_frame_->height;
                frame.channels = 3;

                int data_size = frame.width * frame.height * frame.channels;
                frame.data.resize(data_size);

                // Copy line by line (handle different line sizes/padding)
                for (int y = 0; y < frame.height; y++)
                {
                    memcpy(frame.data.data() + y * frame.width * frame.channels,
                           filtered_frame_->data[0] + y * filtered_frame_->linesize[0],
                           frame.width * frame.channels);
                }

                av_frame_unref(filtered_frame_);
                frame_number_++;
                return true;
            }
        }

        return false;
    }

    void VideoCapture::close()
    {
        cleanup();
    }

} // namespace degirum
