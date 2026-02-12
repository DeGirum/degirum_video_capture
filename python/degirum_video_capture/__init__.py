"""
DeGirum Video Capture

A high-performance video capture library using FFmpeg with Python bindings.
"""

__version__ = "1.0.0"

# Import from the C++ module
from ._video_capture import (
    VideoCapture,
    CAP_PROP_POS_MSEC,
    CAP_PROP_POS_FRAMES,
    CAP_PROP_POS_AVI_RATIO,
    CAP_PROP_FRAME_WIDTH,
    CAP_PROP_FRAME_HEIGHT,
    CAP_PROP_FPS,
    CAP_PROP_FOURCC,
    CAP_PROP_FRAME_COUNT,
)

__all__ = [
    'VideoCapture',
    'CAP_PROP_POS_MSEC',
    'CAP_PROP_POS_FRAMES',
    'CAP_PROP_POS_AVI_RATIO',
    'CAP_PROP_FRAME_WIDTH',
    'CAP_PROP_FRAME_HEIGHT',
    'CAP_PROP_FPS',
    'CAP_PROP_FOURCC',
    'CAP_PROP_FRAME_COUNT',
]
