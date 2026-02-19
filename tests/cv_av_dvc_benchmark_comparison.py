#!/usr/bin/env python3
"""
Benchmark comparison: OpenCV vs PyAV vs DeGirum VideoCapture
"""
import time
import os
import sys
import gc

# Video path
script_dir = os.path.dirname(os.path.abspath(__file__))
VIDEO_PATH = os.path.join(script_dir, "SMPTE_Color_Bars_Large.mp4")

print("=" * 80)
print("Video Decode Benchmark Comparison")
print("=" * 80)
print(f"Video: {os.path.basename(VIDEO_PATH)}")
print()

# ============================================================================
# Benchmark 1: OpenCV
# ============================================================================
try:
    import cv2
    
    print("=" * 80)
    print("1. OpenCV (cv2.VideoCapture)")
    print("=" * 80)
    
    cap = cv2.VideoCapture(VIDEO_PATH)
    if not cap.isOpened():
        print("❌ Failed to open video with OpenCV")
    else:
        frame_count = 0
        start = time.time()
        
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            frame_count += 1
        
        elapsed = time.time() - start
        fps = frame_count / elapsed
        
        cap.release()
        
        print(f"✓ Decoded {frame_count} frames in {elapsed:.3f}s")
        print(f"✓ FPS: {fps:.2f}")
        print()

except ImportError:
    print("=" * 80)
    print("1. OpenCV (cv2.VideoCapture)")
    print("=" * 80)
    print("⚠️  OpenCV not installed (pip install opencv-python)")
    print()

# ============================================================================
# Benchmark 2: PyAV (decode-only, no conversion)
# ============================================================================
try:
    import av
    
    print("=" * 80)
    print("2a. PyAV (av.open) - decode only (YUV)")
    print("=" * 80)
    
    container = av.open(VIDEO_PATH)
    stream = container.streams.video[0]
    stream.thread_type = "AUTO"
    
    frame_count = 0
    start = time.time()
    
    for frame in container.decode(stream):
        # No conversion - raw YUV frame
        frame_count += 1
    
    elapsed = time.time() - start
    fps = frame_count / elapsed
    
    container.close()
    
    print(f"✓ Decoded {frame_count} frames in {elapsed:.3f}s")
    print(f"✓ FPS: {fps:.2f}")
    print()

except ImportError:
    print("=" * 80)
    print("2a. PyAV (av.open) - decode only (YUV)")
    print("=" * 80)
    print("⚠️  PyAV not installed (pip install av)")
    print()

# ============================================================================
# Benchmark 3: PyAV with filter graph (BGR conversion)
# ============================================================================
try:
    import av
    import av.error
    
    print("=" * 80)
    print("2b. PyAV (av.open) - with filter graph (BGR)")
    print("=" * 80)
    
    container = av.open(VIDEO_PATH)
    stream = container.streams.video[0]
    stream.thread_type = "AUTO"
    
    # Create filter graph for format conversion
    graph = av.filter.Graph()
    src = graph.add_buffer(template=stream)
    fmt = graph.add("format", "bgr24")
    sink = graph.add("buffersink")
    
    src.link_to(fmt)
    fmt.link_to(sink)
    graph.configure()
    
    frame_count = 0
    start = time.time()
    
    for frame in container.decode(stream):
        src.push(frame)
        
        while True:
            try:
                out = sink.pull()
                bgr_frame = out.to_ndarray()
                frame_count += 1
            except (av.error.FFmpegError, BlockingIOError):
                break
    
    elapsed = time.time() - start
    fps = frame_count / elapsed
    
    container.close()
    
    print(f"✓ Decoded {frame_count} frames in {elapsed:.3f}s")
    print(f"✓ FPS: {fps:.2f}")
    print()

except ImportError:
    print("=" * 80)
    print("2b. PyAV (av.open) - with filter graph (BGR)")
    print("=" * 80)
    print("⚠️  PyAV not installed (pip install av)")
    print()

# ============================================================================
# Benchmark 3: DeGirum VideoCapture
# ============================================================================
try:
    # Add build directory to path first (prefer latest build)
    build_dir = os.path.join(os.path.dirname(script_dir), 'build', 'python', 'package')
    if os.path.exists(build_dir):
        sys.path.insert(0, build_dir)
    
    from degirum_video_capture import VideoCapture
    
    print("=" * 80)
    print("3. DeGirum VideoCapture (degirum_video_capture)")
    print("=" * 80)
    
    # Run multiple iterations to measure variance
    iterations = 5
    fps_results = []
    
    for i in range(iterations):
        # Force garbage collection before each run for consistent timing
        gc.collect()
        
        cap = VideoCapture(VIDEO_PATH)
        if not cap.is_opened():  # open(VIDEO_PATH):
            print(f"❌ Failed to open video with DeGirum VideoCapture on iteration {i+1}")
            continue
        
        frame_count = 0
        
        # Disable GC during benchmark for consistent timing
        #gc.disable()
        start = time.time()
        
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            # Simulate processing: just access the data to ensure frame is valid
            #_ = frame.shape
            del frame  # Explicitly delete to trigger capsule destructor
            frame_count += 1
        
        elapsed = time.time() - start
        gc.enable()  # Re-enable GC
        fps = frame_count / elapsed
        fps_results.append(fps)
        
        cap.close()
        
        print(f"  Run {i+1}: {frame_count} frames in {elapsed:.3f}s = {fps:.2f} FPS")
    
    if fps_results:
        avg_fps = sum(fps_results) / len(fps_results)
        min_fps = min(fps_results)
        max_fps = max(fps_results)
        print(f"\n✓ Average: {avg_fps:.2f} FPS")
        print(f"✓ Range: {min_fps:.2f} - {max_fps:.2f} FPS")
        print(f"✓ Variance: {max_fps - min_fps:.2f} FPS ({(max_fps - min_fps) / avg_fps * 100:.1f}%)")
        print()

except ImportError as e:
    print("=" * 80)
    print("3. DeGirum VideoCapture (degirum_video_capture)")
    print("=" * 80)
    print(f"⚠️  DeGirum VideoCapture not available: {e}")
    print()

print("=" * 80)
print("Benchmark Complete")
print("=" * 80)
