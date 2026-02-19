"""
Display first, middle, and last frames from a video using VideoCapture.

This script demonstrates how to extract specific frames from a video
processed through the VideoCapture pipeline.
"""

import cv2
import numpy as np
from degirum_video_capture import VideoCapture


def display_key_frames(video_path: str, target_width: int = None, target_height: int = None):
    """
    Display the first, middle, and last frames of a video.
    
    Args:
        video_path: Path to the video file
        target_width: Optional width to resize frames to (None = no resize)
        target_height: Optional height to resize frames to (None = no resize)
    """
    print(f"Processing video: {video_path}")
    
    # Open video
    cap = VideoCapture()
    if not cap.open(video_path):
        print("Failed to open video!")
        return
    
    print(f"Video size: {cap.get(cv2.CAP_PROP_FRAME_WIDTH)}x{cap.get(cv2.CAP_PROP_FRAME_HEIGHT)}")
    
    # Collect all frames
    frames = []
    
    print("Reading frames...")
    while True:
        success, frame = cap.read()
        if not success:
            break
        
        # Resize if target dimensions specified
        if target_width and target_height:
            frame = cv2.resize(frame, (target_width, target_height))
        
        frames.append(frame.copy())
    
    cap.close()
    
    total_frames = len(frames)
    print(f"Total frames: {total_frames}")
    
    if total_frames == 0:
        print("No frames found in video!")
        return
    
    # Get first, middle, and last frame indices
    first_idx = 0
    middle_idx = total_frames // 2
    last_idx = total_frames - 1
    
    print(f"First frame: {first_idx}")
    print(f"Middle frame: {middle_idx}")
    print(f"Last frame: {last_idx}")
    
    # Create a combined display image (3 frames side by side)
    first_frame = frames[first_idx]
    middle_frame = frames[middle_idx]
    last_frame = frames[last_idx]
    
    display_width = first_frame.shape[1]
    display_height = first_frame.shape[0]
    
    # Concatenate frames horizontally
    combined = np.hstack([first_frame, middle_frame, last_frame])
    
    # Add labels to each frame
    font = cv2.FONT_HERSHEY_SIMPLEX
    cv2.putText(combined, f"First (#{first_idx})", (10, 30), font, 0.7, (0, 255, 0), 2)
    cv2.putText(combined, f"Middle (#{middle_idx})", (display_width + 10, 30), font, 0.7, (0, 255, 0), 2)
    cv2.putText(combined, f"Last (#{last_idx})", (2 * display_width + 10, 30), font, 0.7, (0, 255, 0), 2)
    
    # Display the combined image
    cv2.imshow("First, Middle, Last Frames", combined)
    print("\nPress any key to close the window...")
    cv2.waitKey(0)
    cv2.destroyAllWindows()
    
    # Also save individual frames
    cv2.imwrite("/tmp/first_frame.png", first_frame)
    cv2.imwrite("/tmp/middle_frame.png", middle_frame)
    cv2.imwrite("/tmp/last_frame.png", last_frame)
    print("\nFrames saved to /tmp/:")
    print("  - first_frame.png")
    print("  - middle_frame.png")
    print("  - last_frame.png")


if __name__ == "__main__":
    import sys
    
    # Use video path from command line or default
    if len(sys.argv) > 1:
        video_path = sys.argv[1]
    else:
        video_path = "/home/alexander-iakovlev/Workspaces/degirum_video_capture/tests/SMPTE_Color_Bars_Large.mp4"
    
    # Optional width and height arguments for resizing
    target_width = int(sys.argv[2]) if len(sys.argv) > 2 else None
    target_height = int(sys.argv[3]) if len(sys.argv) > 3 else None
    
    display_key_frames(video_path, target_width, target_height)
