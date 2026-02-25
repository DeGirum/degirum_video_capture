"""
DeGirum Video Capture

A high-performance video capture library using FFmpeg with Python bindings.
"""

import os

# Read version from _version.py (generated from CMakeLists.txt)
def _get_version():
    try:
        # Try to import from _version.py in the same directory
        if '__file__' in globals():
            import os
            current_dir = os.path.dirname(__file__)
            version_file = os.path.join(current_dir, '_version.py')
            
            if os.path.exists(version_file):
                version_globals = {}
                with open(version_file, 'r') as f:
                    exec(f.read(), version_globals)
                return version_globals.get('__version__', '0.0.0')
    except Exception:
        pass
    
    # Fallback: try to read from CMakeLists.txt
    try:
        import os
        import re
        
        if '__file__' in globals():
            current_dir = os.path.dirname(os.path.abspath(__file__))
            cmake_file = os.path.join(current_dir, '..', '..', 'CMakeLists.txt')
            
            if os.path.exists(cmake_file):
                with open(cmake_file, 'r') as f:
                    for line in f:
                        if line.strip().startswith('project(') and 'VERSION' in line:
                            match = re.search(r'VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)', line)
                            if match:
                                return match.group(1)
    except Exception:
        pass
    
    return "0.0.0"  # Final fallback

__version__ = _get_version()

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
