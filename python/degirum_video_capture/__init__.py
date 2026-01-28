"""
DeGirum Video Capture

A high-performance video capture library using FFmpeg with Python bindings.
"""

__version__ = "1.0.0"

from ._video_capture import VideoCapture as _VideoCapture
import numpy as np
from typing import Generator, Optional, Tuple

__all__ = ['VideoCapture']


class VideoCapture:
    """
    High-performance video capture class using FFmpeg.
    
    This class provides a generator-based interface for reading video files,
    similar to PyAV but using native FFmpeg with pybind11 bindings.
    
    Example:
        >>> capture = VideoCapture()
        >>> for frame in capture.read_file("video.mp4", 640, 640):
        ...     print(frame.shape)  # (640, 640, 3)
        
    Example with context manager:
        >>> with VideoCapture() as capture:
        ...     capture.open("video.mp4", 640, 640)
        ...     for frame in capture:
        ...         process(frame)
    """
    
    def __init__(self):
        """Initialize VideoCapture object."""
        self._capture = _VideoCapture()
    
    def open(self, filename: str, width: int, height: int) -> bool:
        """
        Open a video file for reading.
        
        Args:
            filename: Path to the video file
            width: Target width for output frames
            height: Target height for output frames
            
        Returns:
            True if successful, False otherwise
        """
        return self._capture.open(filename, width, height)
    
    def read(self) -> Tuple[bool, Optional[np.ndarray]]:
        """
        Read the next frame from the video.
        
        Returns:
            Tuple of (success, frame) where:
                - success (bool): True if a frame was read
                - frame (np.ndarray or None): Frame data (H, W, 3) in BGR format
        """
        return self._capture.read()
    
    def is_opened(self) -> bool:
        """
        Check if the video is opened.
        
        Returns:
            True if opened, False otherwise
        """
        return self._capture.is_opened()
    
    def close(self):
        """Close the video file."""
        self._capture.close()
    
    def get_frame_number(self) -> int:
        """
        Get the current frame number (0-based).
        
        Returns:
            Current frame number
        """
        return self._capture.get_frame_number()
    
    def get_width(self) -> int:
        """
        Get the original video width.
        
        Returns:
            Video width in pixels
        """
        return self._capture.get_width()
    
    def get_height(self) -> int:
        """
        Get the original video height.
        
        Returns:
            Video height in pixels
        """
        return self._capture.get_height()
    
    def read_file(self, filename: str, width: int, height: int) -> Generator[np.ndarray, None, None]:
        """
        Read a video file and yield frames as a generator.
        
        This is the primary method that replicates PyAV's srcq_pyav_fast_bgr functionality.
        Each frame is:
        - Scaled to fit within (width, height) maintaining aspect ratio
        - Padded to exactly (width, height) with centered content
        - Converted to BGR24 format
        - Returned as numpy array of shape (height, width, 3) and dtype uint8
        
        Args:
            filename: Path to the video file
            width: Target width for output frames
            height: Target height for output frames
            
        Yields:
            np.ndarray: Video frames in BGR format, shape (height, width, 3), dtype uint8
            
        Example:
            >>> capture = VideoCapture()
            >>> for frame in capture.read_file("video.mp4", 640, 640):
            ...     assert frame.shape == (640, 640, 3)
            ...     assert frame.dtype == np.uint8
        """
        try:
            if not self.open(filename, width, height):
                raise RuntimeError(f"Failed to open video file: {filename}")
            
            for frame in self._capture:
                yield frame
                
        finally:
            self.close()
    
    def __iter__(self):
        """Return an iterator for reading frames."""
        return iter(self._capture)
    
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_value, traceback):
        """Context manager exit."""
        self.close()

