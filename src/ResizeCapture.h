//
// ResizeCapture C++ Implementation
//
// Copyright 2026 DeGirum Corporation
//

#ifndef RESIZE_CAPTURE_H
#define RESIZE_CAPTURE_H

#include "VideoCapture.h"

extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavutil/opt.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

namespace DG
{
    /// ResizeCapture class that extends VideoCapture with resizing functionality
    /// Inherits from VideoCapture to provide frame resizing during read operations
    class ResizeCapture : public VideoCapture
    {
    public:
        ResizeCapture() = default;
        ResizeCapture(const char *filename, int target_width, int target_height);
        virtual ~ResizeCapture();

        bool open(const char *filename, int target_width, int target_height);

        virtual bool readFrame(AVFrame *dst) override;

        int targetWidth() const { return m_target_width; }
        int targetHeight() const { return m_target_height; }

    private:
        bool initFilterGraph();                      //!< Initialize FFmpeg filter graph for resizing and format conversion
        AVFilterGraph *m_filter_graph = nullptr;     //!< FFmpeg filter graph for resizing and format conversion
        AVFilterContext *m_buffersrc_ctx = nullptr;  //!< Buffer source filter context (input to the graph)
        AVFilterContext *m_buffersink_ctx = nullptr; //!< Buffer sink filter context (output from the graph)
        int m_target_width = 0;                      //!< Target width for resized output
        int m_target_height = 0;                     //!< Target height for resized output
    };

} // namespace DG

#endif // RESIZE_CAPTURE_H
