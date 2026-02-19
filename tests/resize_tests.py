import cv2
import gc
import numpy as np
from typing import Optional, Tuple
import degirum_video_capture as dvc
import time

filename = "images/TrafficExtendedHD.mp4"
#filename = "SMPTE_Color_Bars_Large.mp4"
num_iterations = 5

def read_resize_letterbox(
    cap: cv2.VideoCapture,
    out_size: Tuple[int, int] = (640, 640),   # (width, height)
    pad_color: Tuple[int, int, int] = (114, 114, 114),
    interp_down=cv2.INTER_AREA,
    interp_up=cv2.INTER_LINEAR,
) -> Optional[Tuple[np.ndarray, dict]]:
    """
    Read one frame from an already-open cv2.VideoCapture, resize with aspect ratio preserved,
    then letterbox (pad) to exactly out_size.

    Returns:
        (letterboxed_bgr, meta) on success, or None if no frame available.

    meta contains:
        - original_size: (w0, h0)
        - resized_size: (w1, h1)
        - out_size: (W, H)
        - scale: float (applied uniformly)
        - pad: (left, top, right, bottom)
    """
    ok, frame = cap.read()
    if not ok or frame is None:
        return None

    W, H = out_size
    h0, w0 = frame.shape[:2]

    # Uniform scale to fit within (W, H)
    scale = min(W / w0, H / h0)
    w1 = int(round(w0 * scale))
    h1 = int(round(h0 * scale))

    interp = interp_down if scale < 1.0 else interp_up
    resized = cv2.resize(frame, (w1, h1), interpolation=interp)

    # Compute symmetric padding to reach exact (W, H)
    pad_w = W - w1
    pad_h = H - h1
    left = pad_w // 2
    right = pad_w - left
    top = pad_h // 2
    bottom = pad_h - top

    letterboxed = cv2.copyMakeBorder(
        resized,
        top, bottom, left, right,
        borderType=cv2.BORDER_CONSTANT,
        value=pad_color
    )

    meta = {
        "original_size": (w0, h0),
        "resized_size": (w1, h1),
        "out_size": (W, H),
        "scale": scale,
        "pad": (left, top, right, bottom),
    }
    return letterboxed, meta

def dg_resize(model, cap):
    ok, frame = cap.read()
    if not ok or frame is None:
        return None
    
    return model._preprocessor.forward(frame)


def cv_benchmark():
    cap = cv2.VideoCapture(filename)
    if not cap.isOpened():
        print(f"Failed to open video: {filename}")
        return

    count = 0
    start = time.time()
    while True:
        result = read_resize_letterbox(cap, (640, 640))
        if result is None:
            break
        #letterboxed, meta = result
        #assert letterboxed.shape == (640, 640, 3), f"Unexpected shape: {letterboxed.shape}"
        count += 1

    elapsed = time.time() - start
    fps = count / elapsed
    
    cap.release()

    return count, elapsed, fps

def dg_generator(capture):
    while True:
        ret, frame = capture.read()
        if not ret:
            break
        yield frame

def dvc_benchmark():
        cap = dvc.VideoCapture(filename, 640, 640)

        gc.collect()
        gc.disable()
        frame_count = 0
        start = time.time()

        for frame in dg_generator(cap):
            frame_count += 1
            del frame

        elapsed = time.time() - start
        fps = frame_count / elapsed

        cap.close()
        gc.enable()

        return frame_count, elapsed, fps

def dg_benchmark():
    cap = cv2.VideoCapture(filename)
    if not cap.isOpened():
        print(f"Failed to open video: {filename}")
        return

    import degirum as dg
    model = dg.load_model(
        model_name = "yolov8n_coco--640x640_quant_axelera_metis_1", 
        inference_host_address = "@local", 
        zoo_url = "https://hub.degirum.com/degirum/axelera",
        devices_selected = [0], #,1,2,3],
        #measure_time = True
    )

    count = 0
    start = time.time()
    while True:
        result = dg_resize(model, cap)
        if result is None:
            break
        count += 1

    elapsed = time.time() - start
    fps = count / elapsed
    
    cap.release()

    return count, elapsed, fps

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
    print(f"Average: {total_frames / n:.2f} frames, {total_time / n:.2f}s, {total_frames / total_time:.2f} FPS")

#run_benchmark(dvc_benchmark, "Degirum VideoCapture", num_iterations)
#run_benchmark(cv_benchmark, "OpenCV VideoCapture", num_iterations)
run_benchmark(dg_benchmark, "Degirum + OpenCV VideoCapture", num_iterations)
