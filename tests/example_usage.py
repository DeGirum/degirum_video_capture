"""
Example: Replicate PyAV functionality with degirum_video_capture

This example shows how to replace PyAV's srcq_pyav_fast_bgr with
the equivalent degirum_video_capture implementation.
"""

import time
from degirum_video_capture import VideoCapture

# Video file to process
your_video = "/home/alexander-iakovlev/Workspaces/PySDKExamples/images/Traffic2.mp4"
your_video = "/home/alexander-iakovlev/Workspaces/degirum_video_capture/tests/SMPTE_Color_Bars_Large.mp4"


# Create video capture instance
capture = VideoCapture()

# Process video using the generator pattern
# This is a direct replacement for: srcq_pyav_fast_bgr(your_video, 640, 640)
n = 0
start_time = time.time()

for r in capture.read_file(your_video, 640, 640):
    if n == 10:
            start_time = time.time()
    n += 1

elapsed_time = time.time() - start_time

print(f"\nFPS: {n / elapsed_time:.2f}")