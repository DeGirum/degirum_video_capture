//
// Python Bindings for VideoCapture using pybind11
//
// Copyright 2026 DeGirum Corporation
//

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "../src/VideoCapture.h"
#include "../src/opencv_enums.h"

#define NUM_CHANNELS 3 // BGR24 format has 3 channels

namespace py = pybind11;

namespace DG
{
    /// Convert AVFrame to numpy array with zero-copy (frame lifecycle tied to array)
    ///
    /// This function creates a numpy array that directly references the AVFrame's data buffer.
    /// The AVFrame is kept alive via a Python capsule until the numpy array is garbage collected.
    ///
    /// @param src AVFrame to convert (must be BGR24 format)
    /// @return py::array numpy array referencing the frame data
    py::array frame_to_numpy_bgr(AVFrame *src)
    {
        // Create a reference-counted copy to keep buffer alive
        AVFrame *keep = av_frame_alloc();
        if (av_frame_ref(keep, src) < 0)
            throw std::runtime_error("av_frame_ref failed");

        // Create a capsule that will free the AVFrame when the numpy array is garbage collected
        auto capsule = py::capsule(keep, [](void *p)
                                   {
            AVFrame* f = reinterpret_cast<AVFrame*>(p);
            av_frame_free(&f); });

        // Calculate strides for numpy array (in bytes)
        std::vector<ssize_t> strides = {
            static_cast<ssize_t>(src->linesize[0]),
            static_cast<ssize_t>(NUM_CHANNELS),
            static_cast<ssize_t>(1)};

        // Create and return numpy array
        return py::array(py::buffer_info(
                             src->data[0],                             // pointer to data
                             1,                                        // item size (1 byte for uint8_t)
                             py::format_descriptor<uint8_t>::format(), // format (uint8)
                             3,                                        // height, width, channels
                             {src->height, src->width, NUM_CHANNELS},  // shape of data
                             strides),                                 // strides for each dimension
                         capsule);                                     // Pass the capsule to keep the AVFrame alive
    }

} // namespace DG

// Python module definition using pybind11
PYBIND11_MODULE(_video_capture, m)
{
    m.doc() = "DeGirum Video Capture - FFmpeg-based video reading library";

    py::class_<DG::VideoCapture>(m, "VideoCapture")
        // Default constructor (no args) -> VideoCapture
        .def(py::init([]()
                      { return std::make_unique<DG::VideoCapture>(); }),
             "Create a new VideoCapture object")

        // Constructor with filename only -> VideoCapture
        .def(py::init([](const char *filename)
                      { return std::make_unique<DG::VideoCapture>(filename); }),
             py::arg("filename"),
             "Create and open a video file\n\n"
             "Args:\n"
             "    filename (str): Path to the video file")

        // Constructor with filename, width, height -> VideoCapture with resize
        .def(py::init([](const char *filename, int width, int height)
                      { return std::make_unique<DG::VideoCapture>(filename, width, height); }),
             py::arg("filename"), py::arg("width"), py::arg("height"),
             "Create and open a video file with resizing\n\n"
             "Args:\n"
             "    filename (str): Path to the video file\n"
             "    width (int): Target width for resized frames\n"
             "    height (int): Target height for resized frames")

        .def("open", &DG::VideoCapture::open,
             py::arg("filename"), py::arg("width") = 0, py::arg("height") = 0,
             "Open a video file for reading\n\n"
             "Args:\n"
             "    filename (str): Path to the video file\n"
             "    width (int, optional): Target width for resized frames (default: 0 = no resize)\n"
             "    height (int, optional): Target height for resized frames (default: 0 = no resize)\n\n"
             "Returns:\n"
             "    bool: True if successful, False otherwise")

        .def("read", [](DG::VideoCapture &self)
             {
                // Check if video is opened
                if (!self.isOpened()) {
                    return py::make_tuple(false, py::none());
                }

                // Allocate BGR frame for this read
                AVFrame *bgr_frame = av_frame_alloc();
                if (!bgr_frame) {
                    throw std::runtime_error("Failed to allocate AVFrame");
                }

                // Configure frame for BGR24
                bgr_frame->format = AV_PIX_FMT_BGR24;
                bgr_frame->width = self.outputWidth();
                bgr_frame->height = self.outputHeight();

                // Allocate buffer for the frame data
                if (av_frame_get_buffer(bgr_frame, 32) < 0) {
                    av_frame_free(&bgr_frame);
                    throw std::runtime_error("Failed to allocate frame buffer");
                }

                // Read frame
                if (!self.readFrame(bgr_frame)) {
                    av_frame_free(&bgr_frame);
                    return py::make_tuple(false, py::none());
                }

                try {
                    // Convert to numpy with zero-copy (frame_to_numpy_bgr creates reference)
                    auto array = DG::frame_to_numpy_bgr(bgr_frame);
                    
                    // Free original frame since numpy array has its own reference
                    av_frame_free(&bgr_frame);
                    
                    return py::make_tuple(true, array);
                }
                catch (const std::exception& e) {
                    av_frame_free(&bgr_frame);
                    throw;
                } }, "Read the next frame from the video\n\n"
                  "Returns:\n"
                  "    tuple: (success: bool, frame: np.ndarray or None)\n"
                  "           success is True if a frame was read\n"
                  "           frame is numpy array (height, width, 3) in BGR format or None")

        .def("isOpened", &DG::VideoCapture::isOpened, "Check if the video is opened\n\n"
                                                      "Returns:\n"
                                                      "    bool: True if opened, False otherwise")

        .def("close", &DG::VideoCapture::close, "Close the video file")

        .def("get", &DG::VideoCapture::get, py::arg("prop_id"), "Get video capture property\n\n"
                                                                "Args:\n"
                                                                "    prop_id (int): Property identifier (use CAP_PROP_* constants)\n\n"
                                                                "Returns:\n"
                                                                "    float: Property value, or -1 if not supported/available\n\n"
                                                                "Example:\n"
                                                                "    fps = cap.get(CAP_PROP_FPS)\n"
                                                                "    frame_count = cap.get(CAP_PROP_FRAME_COUNT)")

        .def("__enter__", [](DG::VideoCapture &self) -> DG::VideoCapture &
             { return self; }, "Context manager entry")

        .def("__exit__", [](DG::VideoCapture &self, py::object, py::object, py::object)
             { self.close(); }, "Context manager exit");

    // Expose VideoCaptureProperties enum constants
    m.attr("CAP_PROP_POS_MSEC") = static_cast<int>(cv::CAP_PROP_POS_MSEC);
    m.attr("CAP_PROP_POS_FRAMES") = static_cast<int>(cv::CAP_PROP_POS_FRAMES);
    m.attr("CAP_PROP_POS_AVI_RATIO") = static_cast<int>(cv::CAP_PROP_POS_AVI_RATIO);
    m.attr("CAP_PROP_FRAME_WIDTH") = static_cast<int>(cv::CAP_PROP_FRAME_WIDTH);
    m.attr("CAP_PROP_FRAME_HEIGHT") = static_cast<int>(cv::CAP_PROP_FRAME_HEIGHT);
    m.attr("CAP_PROP_FPS") = static_cast<int>(cv::CAP_PROP_FPS);
    m.attr("CAP_PROP_FOURCC") = static_cast<int>(cv::CAP_PROP_FOURCC);
    m.attr("CAP_PROP_FRAME_COUNT") = static_cast<int>(cv::CAP_PROP_FRAME_COUNT);

    m.attr("__version__") = "1.0.0";
}
