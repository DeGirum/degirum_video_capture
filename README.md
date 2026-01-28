# DeGirum Video Capture

High-performance video capture library using FFmpeg with Python bindings. Replacement for PyAV's video reading functionality minus the FFmpeg conflicts with OpenCV by packaging FFmpeg statically.

## Installation

Build from source and install the wheel:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
pip install wheels/degirum_video_capture-*.whl
```

**Prerequisites**: CMake 3.16+, C++17 compiler, Python 3.8+, build tools (make, autoconf, pkg-config, nasm)

Ubuntu/Debian:
```bash
sudo apt-get install build-essential cmake git python3-dev python3-numpy \
    autoconf automake libtool pkg-config nasm yasm
```

## Usage

### Basic Example

```python
from degirum_video_capture import VideoCapture

capture = VideoCapture()
for frame in capture.read_file("video.mp4", 640, 640):
    # frame is numpy array (640, 640, 3) in BGR format
    print(f"Frame: {frame.shape}, {frame.dtype}")
```

### Replacing PyAV Code

**Before (PyAV):**
```python
import av

def srcq_pyav_fast_bgr(inp, w, h):
    container = av.open(inp)
    stream = container.streams.video[0]
    stream.thread_type = "AUTO"
    
    graph = av.filter.Graph()
    src = graph.add_buffer(template=stream)
    scale = graph.add("scale", f"{w}:{h}:force_original_aspect_ratio=decrease")
    pad = graph.add("pad", f"{w}:{h}:(ow-iw)/2:(oh-ih)/2")
    fmt = graph.add("format", "bgr24")
    sink = graph.add("buffersink")
    
    src.link_to(scale)
    scale.link_to(pad)
    pad.link_to(fmt)
    fmt.link_to(sink)
    graph.configure()
    
    for frame in container.decode(stream):
        src.push(frame)
        while True:
            try:
                out = sink.pull()
            except (av.error.FFmpegError, BlockingIOError):
                break
            yield out.to_ndarray()

for frame in srcq_pyav_fast_bgr("video.mp4", 640, 640):
    process(frame)
```

**After (degirum_video_capture):**
```python
from degirum_video_capture import VideoCapture

capture = VideoCapture()
for frame in capture.read_file("video.mp4", 640, 640):
    process(frame)
```

### Context Manager

```python
from degirum_video_capture import VideoCapture

with VideoCapture() as capture:
    capture.open("video.mp4", 640, 640)
    for frame in capture:
        process_frame(frame)
```

## API Reference

### VideoCapture

- `__init__()` - Create VideoCapture object
- `open(filename: str, width: int, height: int) -> bool` - Open video file
- `read() -> Tuple[bool, Optional[np.ndarray]]` - Read next frame
- `read_file(filename: str, width: int, height: int) -> Generator` - Read as generator
- `is_opened() -> bool` - Check if video is open
- `close()` - Close video file
- `get_frame_number() -> int` - Get current frame number
- `get_width() -> int` - Get original video width
- `get_height() -> int` - Get original video height

**Frame Format**: All frames returned as `numpy.ndarray` with shape `(height, width, 3)`, dtype `uint8`, BGR color format.

**Scaling**: Frames are scaled to fit within specified dimensions while maintaining aspect ratio, then padded to exact size with centered content.

## Building from Source

### Quick Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

Tests run automatically by default. To disable:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DRUN_TESTS=OFF
```

### What Gets Built

1. FFmpeg 7.1 from source (static libraries)
2. pybind11 v2.13.6 Python bindings
3. C++ VideoCapture library
4. Python wheel package
5. Automated test suite (if enabled)

**Output**: `build/wheels/degirum_video_capture-*.whl`

### Manual Testing

```bash
cd build
cmake --build . --target test_module
```

Or with custom video:
```bash
PYTHONPATH=build/python/package python3 tests/test_video_capture.py /path/to/video.mp4
```

## Technical Details

### Architecture

- **C++ Layer**: FFmpeg-based video decoder with filter graph (scale→pad→format)
- **Python Bindings**: pybind11 with numpy integration and zero-copy where possible
- **Static Linking**: All FFmpeg libraries (.a) bundled into single .so Python module

## Troubleshooting

**FFmpeg build fails**: Check `build/extern/_BuildExternalDependency/ffmpeg-build/config.log`. Ensure all build tools installed.

**Module not found**: Set PYTHONPATH before importing:
```bash
export PYTHONPATH=/path/to/build/python/package:$PYTHONPATH
```

**Linking errors**: Install additional system libraries:
```bash
sudo apt-get install zlib1g-dev libbz2-dev liblzma-dev
```

## License

Copyright 2026 DeGirum Corporation. All rights reserved.

