//
// VideoCapture C++ Implementation
//
// Copyright 2026 DeGirum Corporation
//

#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
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
        ~VideoCapture();

        bool open(const char *filename);
        void close();
        bool isOpened() const;

        int width() const;
        int height() const;
        AVPixelFormat srcPixelFormat() const;

        bool readFrame(AVFrame *dst);

    private:
        bool receiveAndConvert(AVFrame *dst);

        AVFormatContext *m_fmt_ctx = nullptr;          //!< FFmpeg format context for input video
        AVCodecContext *m_codec_ctx = nullptr;         //!< FFmpeg codec context for decoding video frames
        SwsContext *m_sws_ctx = nullptr;               //!< Swscale context for pixel format conversion
        AVFrame *m_yuv_frame = nullptr;                //!< Internal frame for decoded YUV data
        int m_video_stream_index = -1;                 //!< Index of the video stream in the input file (file contains multiple streams like audio/subtitles)
        int m_width = 0;                               //!< Video width in pixels (from codec parameters)
        int m_height = 0;                              //!< Video height in pixels (from codec parameters)
        AVPixelFormat m_src_pix_fmt = AV_PIX_FMT_NONE; //!< Source pixel format of the video frames (from codec parameters)
        bool m_flush_pending = false;                  //!< Flag to indicate if we've sent the flush packet to the decoder after reaching end of file
    };

} // namespace DG

#endif // VIDEO_CAPTURE_H
