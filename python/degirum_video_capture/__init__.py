"""
DeGirum Video Capture

A high-performance video capture library using FFmpeg with Python bindings.
"""

__version__ = "1.0.0"

# Import from the C++ module
from ._video_capture import VideoCapture

__all__ = ['VideoCapture']
