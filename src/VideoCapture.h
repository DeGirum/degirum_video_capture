//
// VideoCapture C++ Implementation
//
// Copyright 2026 DeGirum Corporation
//

#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace DG
{
    /// C++ VideoCapture class following OpenCV-like interface for video reading using FFmpeg
    /// Exposed to the user via Python bindings in python/video_capture_bindings.cpp
    class VideoCapture
    {
    public:
        VideoCapture() = default;
        explicit VideoCapture(const char *filename);
        explicit VideoCapture(const char *filename, int target_width, int target_height);
        ~VideoCapture();

        bool open(const char *filename, int target_width = 0, int target_height = 0);
        void close();
        bool isOpened() const;

        double get(int propId) const;

        int outputWidth() const { return m_target_width > 0 ? m_target_width : m_width; }
        int outputHeight() const { return m_target_height > 0 ? m_target_height : m_height; }

        bool readFrame(AVFrame *dst);

    private:
        // Common functions and variables
        bool receiveAndConvert(AVFrame *dst);

        AVFormatContext *m_fmt_ctx = nullptr;                                              //!< FFmpeg format context for input video
        AVCodecContext *m_codec_ctx = nullptr;                                             //!< FFmpeg codec context for decoding video frames
        SwsContext *m_sws_ctx = nullptr;                                                   //!< Swscale context for pixel format conversion
        AVFrame *m_yuv_frame = nullptr;                                                    //!< Internal frame for decoded YUV data
        int m_video_stream_index = -1;                                                     //!< Index of the video stream in the input file (file contains multiple streams like audio/subtitles)
        int m_width = 0;                                                                   //!< Video width in pixels (from codec parameters)
        int m_height = 0;                                                                  //!< Video height in pixels (from codec parameters)
        AVPixelFormat m_src_pix_fmt = AV_PIX_FMT_NONE;                                     //!< Source pixel format of the video frames (from codec parameters)
        bool m_flush_pending = false;                                                      //!< Flag to indicate if we've sent the flush packet to the decoder after reaching end of file
        int64_t m_frame_count = 0;                                                         //!< Current frame position (number of frames read so far)
        int64_t m_last_pts = AV_NOPTS_VALUE;                                               //!< PTS of the last decoded frame
        bool (VideoCapture::*m_readFrameImpl)(AVFrame *) = &VideoCapture::readFrameDirect; //!< Pointer to the current read frame implementation (direct or filtered)

        // Functions only used WITHOUT filter graph
        bool readFrameDirect(AVFrame *dst); //!< Read frame without resizing, direct YUV->BGR conversion

        // Functions + variables for filter graph, used only when resizing+padding is enabled
        bool initFilterGraph();                      //!< Initialize FFmpeg filter graph for resizing and format conversion
        bool readFrameFiltered(AVFrame *dst);        //!< Read frame with resizing and format conversion
        AVFilterGraph *m_filter_graph = nullptr;     //!< FFmpeg filter graph for resizing and format conversion
        AVFilterContext *m_buffersrc_ctx = nullptr;  //!< Buffer source filter context (input to the graph)
        AVFilterContext *m_buffersink_ctx = nullptr; //!< Buffer sink filter context (output from the graph)
        int m_target_width = 0;                      //!< Target width for resized output
        int m_target_height = 0;                     //!< Target height for resized output
    };

} // namespace DG

#endif // VIDEO_CAPTURE_H
