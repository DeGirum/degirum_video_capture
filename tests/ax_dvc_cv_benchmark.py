import gc
#import degirum
import degirum_video_capture as dvc
import cv2
import numpy as np
import time

#filename = "images/TrafficExtendedHD.mp4"
filename = "SMPTE_Color_Bars_Large.mp4"
model_name = "yolov8n_coco--640x640_quant_axelera_metis_1"
num_iterations = 5

def dg_generator(capture):
    while True:
        ret, frame = capture.read()
        if not ret:
            break
        yield frame

def dummy_generator(img):
    for _ in range(5360):
        yield img

def loadModel():
    import degirum as dg
    model = dg.load_model(
        model_name = model_name, 
        inference_host_address = "@local", 
        zoo_url = "https://hub.degirum.com/degirum/axelera",
        devices_selected = [0,1,2,3],
        #measure_time = True
    )

    dummy_image = np.zeros((10, 10, 3), dtype=np.uint8)
    preprocessed_image = model._preprocessor.forward(dummy_image)[0]

    return model, preprocessed_image

def cv2_benchmark():
        cap = cv2.VideoCapture(filename)

        frame_count = 0
        start = time.time()

        for frame in dg_generator(cap):
            frame_count += 1

        elapsed = time.time() - start
        fps = frame_count / elapsed

        cap.release()

        return frame_count, elapsed, fps


def cv2_ax_benchmark():
        cap = cv2.VideoCapture(filename)

        frame_count = 0
        start = time.time()

        for result in model.predict_batch(dg_generator(cap)):
            frame_count += 1

        elapsed = time.time() - start
        fps = frame_count / elapsed

        cap.release()

        return frame_count, elapsed, fps


def dvc_benchmark():
        #cap = dvc.VideoCapture()
        #cap.open(filename)
        cap = dvc.VideoCapture(filename) #, 640, 640)
        #cap = degirum.video_capture.VideoCapture(filename) #, 640, 640)

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

def dvc_ax_benchmark():
        cap = dvc.VideoCapture()
        cap.open(filename)

        gc.collect()
        frame_count = 0
        start = time.time()

        for result in model.predict_batch(dg_generator(cap)):
                frame_count += 1

        elapsed = time.time() - start
        fps = frame_count / elapsed

        cap.close()

        return frame_count, elapsed, fps

def ax_benchmark():
        frame_count = 0
        start = time.time()

        for result in model.predict_batch(dummy_generator(preprocessed_image)):
                frame_count += 1

        elapsed = time.time() - start
        fps = frame_count / elapsed

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
      
#model, preprocessed_image = loadModel()
#run_benchmark(cv2_benchmark, "OpenCV", num_iterations)
#run_benchmark(cv2_ax_benchmark, "OpenCV Axelera", num_iterations)
run_benchmark(dvc_benchmark, "Degirum VideoCapture", num_iterations)
#run_benchmark(dvc_ax_benchmark, "Degirum VideoCapture Axelera", num_iterations)
#run_benchmark(ax_benchmark, "Axelera Only", num_iterations)
