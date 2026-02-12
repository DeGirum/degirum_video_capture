"""
Test script for degirum_video_capture

This script demonstrates the usage and tests the functionality
of the VideoCapture class.
"""

import sys
import os
import time
import numpy as np

# Default test video - use absolute path relative to this script
DEFAULT_VIDEO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "SMPTE_Color_Bars.mp4")

# Import the module
try:
    from degirum_video_capture import VideoCapture, CAP_PROP_FRAME_WIDTH, CAP_PROP_FRAME_HEIGHT
    print("✓ Successfully imported VideoCapture")
except ImportError as e:
    print(f"✗ Failed to import VideoCapture: {e}")
    sys.exit(1)


def test_basic_functionality():
    """Test basic VideoCapture functionality"""
    print("\n=== Testing Basic Functionality ===")
    
    # Test initialization
    capture = VideoCapture()
    print("✓ VideoCapture initialized")
    
    # Test that it's not opened
    assert not capture.isOpened(), "Capture should not be opened initially"
    print("✓ isOpened() returns False for new capture")
    
    print("\n✓ All basic functionality tests passed")


def test_video_reading(video_path, width=640, height=640):
    """Test video reading functionality"""
    print(f"\n=== Testing Video Reading ===")
    print(f"Video: {video_path}")
    print(f"Target size: {width}x{height}")
    
    try:
        frame_count = 0
        with VideoCapture() as capture:
            if not capture.open(video_path, width, height):
                print(f"✗ Failed to open video: {video_path}")
                return False
            
            while True:
                success, frame = capture.read()
                if not success:
                    break
                frame_count += 1
                
                # Validate frame properties
                assert isinstance(frame, np.ndarray), "Frame should be numpy array"
                assert frame.shape == (height, width, 3), f"Frame shape should be ({height}, {width}, 3)"
                assert frame.dtype == np.uint8, "Frame dtype should be uint8"
                
                if frame_count == 1:
                    print(f"✓ First frame: shape={frame.shape}, dtype={frame.dtype}")
                    print(f"  - Min value: {frame.min()}")
                    print(f"  - Max value: {frame.max()}")
                    print(f"  - Mean value: {frame.mean():.2f}")
                
                # Only process first few frames for testing
                if frame_count >= 10:
                    print(f"✓ Processed {frame_count} frames successfully")
                    break
        
        if frame_count == 0:
            print("⚠ No frames were read from the video")
        else:
            print(f"\n✓ Video reading test passed ({frame_count} frames processed)")
            
    except Exception as e:
        print(f"✗ Error during video reading: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    return True


def test_context_manager(video_path, width=640, height=640):
    """Test context manager functionality"""
    print(f"\n=== Testing Context Manager ===")
    
    try:
        frame_count = 0
        with VideoCapture() as capture:
            if not capture.open(video_path, width, height):
                print(f"✗ Failed to open video: {video_path}")
                return False
            
            print(f"✓ Video opened successfully")
            print(f"  - Original size: {int(capture.get(CAP_PROP_FRAME_WIDTH))}x{int(capture.get(CAP_PROP_FRAME_HEIGHT))}")
            
            while True:
                success, frame = capture.read()
                if not success:
                    break
                frame_count += 1
                if frame_count >= 5:
                    break
        
        print(f"✓ Context manager test passed ({frame_count} frames)")
        return True
        
    except Exception as e:
        print(f"✗ Error in context manager test: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_manual_reading(video_path, width=640, height=640):
    """Test manual frame reading"""
    print(f"\n=== Testing Manual Reading ===")
    
    capture = VideoCapture()
    
    try:
        if not capture.open(video_path, width, height):
            print(f"✗ Failed to open video: {video_path}")
            return False
        
        print(f"✓ Video opened")
        
        # Read a few frames manually
        for i in range(5):
            success, frame = capture.read()
            if not success:
                print(f"✗ Failed to read frame {i}")
                break
            
            assert frame.shape == (height, width, 3)
            print(f"✓ Frame {i}: {frame.shape}")
        
        capture.close()
        print("✓ Manual reading test passed")
        return True
        
    except Exception as e:
        print(f"✗ Error in manual reading test: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        capture.close()


def test_fps_benchmark(video_path, width=640, height=640):
    """Test FPS performance benchmark"""
    print(f"\n=== Testing FPS Performance ===")
    print(f"Video: {video_path}")
    print(f"Target size: {width}x{height}")
    
    capture = VideoCapture()
    
    try:
        if not capture.open(video_path, width, height):
            print(f"✗ Failed to open video: {video_path}")
            return False
        
        n = 0
        start_time = time.time()
        
        while True:
            success, frame = capture.read()
            if not success:
                break
            n += 1
        
        elapsed_time = time.time() - start_time
        fps = n / elapsed_time if elapsed_time > 0 else 0
        
        capture.close()
        
        print(f"DIAGNOSTIC: Second fresh read: {n} frames")
        print(f"✓ Processed {n} frames in {elapsed_time:.2f} seconds")
        print(f"✓ FPS: {fps:.2f}")
        return True
        
    except Exception as e:
        print(f"✗ Error during FPS benchmark: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        capture.close()


def main():
    """Main test function"""
    print("=" * 60)
    print("DeGirum Video Capture - Test Suite")
    print("=" * 60)
    
    # Test basic functionality
    test_basic_functionality()
    
    # Determine video file to use
    if len(sys.argv) >= 2:
        video_path = sys.argv[1]
    else:
        video_path = DEFAULT_VIDEO
        if not os.path.exists(video_path):
            print("\n" + "=" * 60)
            print("WARNING: Default video not found:")
            print(f"  {DEFAULT_VIDEO}")
            print("\nTo test video reading, provide a video file path:")
            print(f"  python {sys.argv[0]} <video_file.mp4>")
            print("=" * 60)
            return
        print(f"\nUsing default video: {video_path}")
    
    # Run video tests
    all_passed = True
    all_passed &= test_video_reading(video_path)
    all_passed &= test_context_manager(video_path)
    all_passed &= test_manual_reading(video_path)
    all_passed &= test_fps_benchmark(video_path)
    
    # Summary
    print("\n" + "=" * 60)
    if all_passed:
        print("✓ ALL TESTS PASSED")
    else:
        print("✗ SOME TESTS FAILED")
    print("=" * 60)


if __name__ == "__main__":
    main()

