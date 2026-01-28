//
// VideoCapture C++ Implementation
//
// Copyright 2026 DeGirum Corporation
//

#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace degirum
{

    /**
     * @brief Video frame structure holding BGR image data
     */
    struct VideoFrame
    {
        std::vector<uint8_t> data; ///< Frame data in BGR format
        int width;                 ///< Frame width
        int height;                ///< Frame height
        int channels;              ///< Number of channels (3 for BGR)

        VideoFrame() : width(0), height(0), channels(3) {}

        VideoFrame(int w, int h, int c = 3)
            : width(w), height(h), channels(c)
        {
            data.resize(w * h * c);
        }
    };

    /**
     * @brief VideoCapture class for reading video files using FFmpeg
     *
     * This class replicates the functionality of PyAV's srcq_pyav_fast_bgr,
     * providing frame-by-frame video reading with scaling, padding, and
     * format conversion to BGR.
     */
    class VideoCapture
    {
    public:
        /**
         * @brief Constructor
         */
        VideoCapture();

        /**
         * @brief Destructor
         */
        ~VideoCapture();

        /**
         * @brief Open a video file for reading
         * @param filename Path to the video file
         * @param width Target width for output frames
         * @param height Target height for output frames
         * @return true if successful, false otherwise
         */
        bool open(const std::string &filename, int width, int height);

        /**
         * @brief Read the next frame from the video
         * @param frame Output frame in BGR format
         * @return true if a frame was read, false if end of stream or error
         */
        bool read(VideoFrame &frame);

        /**
         * @brief Check if the video is opened
         * @return true if opened, false otherwise
         */
        bool isOpened() const { return is_opened_; }

        /**
         * @brief Close the video file
         */
        void close();

        /**
         * @brief Get the current frame number
         * @return Frame number (0-based)
         */
        int getFrameNumber() const { return frame_number_; }

        /**
         * @brief Get video width
         * @return Width of the video stream
         */
        int getWidth() const { return video_width_; }

        /**
         * @brief Get video height
         * @return Height of the video stream
         */
        int getHeight() const { return video_height_; }

    private:
        // FFmpeg structures
        AVFormatContext *format_ctx_;
        AVCodecContext *codec_ctx_;
        AVFilterGraph *filter_graph_;
        AVFilterContext *buffersrc_ctx_;
        AVFilterContext *buffersink_ctx_;
        AVStream *video_stream_;

        // Reusable frame/packet structures
        AVPacket *packet_;
        AVFrame *av_frame_;
        AVFrame *filtered_frame_;

        // State
        bool is_opened_;
        int video_stream_index_;
        int target_width_;
        int target_height_;
        int video_width_;
        int video_height_;
        int frame_number_;

        // Helper methods
        bool initFilterGraph();
        void cleanup();
    };

} // namespace degirum

#endif // VIDEO_CAPTURE_H
