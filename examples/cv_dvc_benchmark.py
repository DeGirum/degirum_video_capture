import degirum_video_capture as dvc
import cv2
import gc
import time

filename = "SMPTE_Color_Bars_Large.mp4"
num_iterations = 5

def frame_generator(capture):
    while True:
        ret, frame = capture.read()
        if not ret:
            break
        yield frame

def cv2_benchmark():
    cap = cv2.VideoCapture(filename)

    frame_count = 0
    start = time.time()

    for frame in frame_generator(cap):
        frame_count += 1

    elapsed = time.time() - start
    fps = frame_count / elapsed

    cap.release()

    return frame_count, elapsed, fps

def dvc_benchmark():
    cap = dvc.VideoCapture(filename)

    gc.collect()
    gc.disable()
    frame_count = 0
    start = time.time()

    for frame in frame_generator(cap):
        frame_count += 1
        del frame

    elapsed = time.time() - start
    fps = frame_count / elapsed

    cap.close()
    gc.enable()
    gc.collect()

    return frame_count, elapsed, fps

def dvc_crop_benchmark():
    cap = dvc.VideoCapture(filename, 640, 640)

    gc.collect()
    gc.disable()
    frame_count = 0
    start = time.time()

    for frame in frame_generator(cap):
        frame_count += 1
        del frame

    elapsed = time.time() - start
    fps = frame_count / elapsed

    cap.close()
    gc.enable()
    gc.collect()

    return frame_count, elapsed, fps

def run_benchmark(func, name, n):
    print("================================")
    print(f"{name} Benchmark:")
    total_time = 0
    total_frames = 0
    for _ in range(n):
        frame_count, elapsed, fps = func()
        total_time += elapsed
        total_frames += frame_count
        print(f"Frames: {frame_count}, Elapsed time: {elapsed:.2f}s, FPS: {fps:.2f}")

run_benchmark(cv2_benchmark, "OpenCV", num_iterations)
run_benchmark(dvc_benchmark, "Degirum VideoCapture", num_iterations)
run_benchmark(dvc_crop_benchmark, "Degirum VideoCapture (Crop 640x640)", num_iterations)