//
// Python Bindings for VideoCapture using pybind11
//
// Copyright 2026 DeGirum Corporation
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "VideoCapture.h"

namespace py = pybind11;

namespace degirum
{

    /**
     * @brief Iterator class for VideoCapture to enable Python iteration
     */
    class VideoCaptureIterator
    {
    public:
        explicit VideoCaptureIterator(VideoCapture *capture)
            : capture_(capture), done_(false) {}

        py::array_t<uint8_t> next()
        {
            if (done_ || !capture_ || !capture_->isOpened())
            {
                throw py::stop_iteration();
            }

            VideoFrame frame;
            if (!capture_->read(frame))
            {
                done_ = true;
                throw py::stop_iteration();
            }

            // Create numpy array from frame data
            // Shape: (height, width, channels)
            std::vector<ssize_t> shape = {frame.height, frame.width, frame.channels};
            std::vector<ssize_t> strides = {
                frame.width * frame.channels, // row stride
                frame.channels,               // column stride
                1                             // pixel component stride
            };

            // Create array (this will copy the data)
            return py::array_t<uint8_t>(shape, strides, frame.data.data());
        }

    private:
        VideoCapture *capture_;
        bool done_;
    };

} // namespace degirum

PYBIND11_MODULE(_video_capture, m)
{
    m.doc() = "DeGirum Video Capture - FFmpeg-based video reading library";

    // VideoFrame class
    py::class_<degirum::VideoFrame>(m, "VideoFrame")
        .def(py::init<>())
        .def(py::init<int, int, int>(),
             py::arg("width"), py::arg("height"), py::arg("channels") = 3)
        .def_readonly("width", &degirum::VideoFrame::width,
                      "Frame width")
        .def_readonly("height", &degirum::VideoFrame::height,
                      "Frame height")
        .def_readonly("channels", &degirum::VideoFrame::channels,
                      "Number of channels (3 for BGR)")
        .def_property_readonly("data", [](const degirum::VideoFrame &frame)
                               {
                // Return numpy array view of the frame data
                std::vector<ssize_t> shape = {frame.height, frame.width, frame.channels};
                std::vector<ssize_t> strides = {
                    frame.width * frame.channels,
                    frame.channels,
                    1
                };
                return py::array_t<uint8_t>(shape, strides, frame.data.data()); }, "Frame data as numpy array (height, width, channels) in BGR format");

    // VideoCaptureIterator class
    py::class_<degirum::VideoCaptureIterator>(m, "VideoCaptureIterator")
        .def("__iter__", [](degirum::VideoCaptureIterator &it) -> degirum::VideoCaptureIterator &
             { return it; })
        .def("__next__", &degirum::VideoCaptureIterator::next);

    // VideoCapture class
    py::class_<degirum::VideoCapture>(m, "VideoCapture")
        .def(py::init<>(),
             "Create a new VideoCapture object")

        .def("open", &degirum::VideoCapture::open,
             py::arg("filename"), py::arg("width"), py::arg("height"),
             "Open a video file for reading\n\n"
             "Args:\n"
             "    filename (str): Path to the video file\n"
             "    width (int): Target width for output frames\n"
             "    height (int): Target height for output frames\n\n"
             "Returns:\n"
             "    bool: True if successful, False otherwise")

        .def("read", [](degirum::VideoCapture &self)
             {
                degirum::VideoFrame frame;
                bool success = self.read(frame);
                if (!success) {
                    return py::make_tuple(false, py::none());
                }
                
                // Convert frame to numpy array
                std::vector<ssize_t> shape = {frame.height, frame.width, frame.channels};
                std::vector<ssize_t> strides = {
                    frame.width * frame.channels,
                    frame.channels,
                    1
                };
                auto array = py::array_t<uint8_t>(shape, strides, frame.data.data());
                
                return py::make_tuple(true, array); }, "Read the next frame from the video\n\n"
                  "Returns:\n"
                  "    tuple: (success: bool, frame: np.ndarray or None)\n"
                  "           success is True if a frame was read\n"
                  "           frame is numpy array (height, width, 3) in BGR format or None")

        .def("is_opened", &degirum::VideoCapture::isOpened, "Check if the video is opened\n\n"
                                                            "Returns:\n"
                                                            "    bool: True if opened, False otherwise")

        .def("close", &degirum::VideoCapture::close, "Close the video file")

        .def("get_frame_number", &degirum::VideoCapture::getFrameNumber, "Get the current frame number (0-based)\n\n"
                                                                         "Returns:\n"
                                                                         "    int: Current frame number")

        .def("get_width", &degirum::VideoCapture::getWidth, "Get the original video width\n\n"
                                                            "Returns:\n"
                                                            "    int: Video width in pixels")

        .def("get_height", &degirum::VideoCapture::getHeight, "Get the original video height\n\n"
                                                              "Returns:\n"
                                                              "    int: Video height in pixels")

        .def("__iter__", [](degirum::VideoCapture &self)
             { return degirum::VideoCaptureIterator(&self); }, "Return an iterator for reading frames")

        .def("__enter__", [](degirum::VideoCapture &self) -> degirum::VideoCapture &
             { return self; }, "Context manager entry")

        .def("__exit__", [](degirum::VideoCapture &self, py::object exc_type, py::object exc_value, py::object traceback)
             { self.close(); }, "Context manager exit");

    // Module-level version info
    m.attr("__version__") = "1.0.0";
}
