# degirum_video_capture - DeGirum video reading and decoding library

This package is intended to replace OpenCV's `cv2.VideoCapture()` class for the purposes of video reading and decoding in performance-based pipelines.
The goals of this project are to:

1. Provide more frames per second than `cv2.VideoCapture.read()`
2. Provide utility by allowing the user to enable select filters when reading a video
3. Remaining compatible with in-process `opencv-python` by avoiding ffmpeg libaray symbol conflict

## License

[MIT License](https://github.com/DeGirum/degirum_video_capture/blob/master/LICENSE)

[External Licenses](https://github.com/DeGirum/degirum_video_capture/blob/master/LICENSES)

## Installation

`pip install degirum-video-capture`

## Documentation

### Class `VideoCapture`

> **Note**: `VideoCapture` can be used as a context manager

#### def `__init__`( \[source\], \[filter args\] )
> **ARGS**
> * *optional* (string) `source`: source to read from. Acceptable values: filepath, rtsp stream, usb camera
> * *optional* filter args:
>    * (int) `width`: Width to which the output frame will be resized and padded to. Aspect ratio will be maintained.
>    * (int) `height`: Height to which the output frame will be resized and padded to. Aspect ratio will be maintained.
>
> **RETURNS**
> * `VideoCapture` object

#### def `open`( source, \[filter args\] )
> **ARGS**
> * (string) `source`: same as `source` in `__init__`
> * *optional* filter args: same as filter args in `__init__`
> > Note: `open()` does not need to be explicitly called if `source` was provided in `__init__`.
>
> **RETURNS**
> * True if `source` opened successfully. False otherwise.

#### def `read`()
> **RETURNS**
> * tuple: (`success`: bool, `frame`: np.ndarray or None)
>   * `success`: True if a frame was read, False otherwise.
>   * `frame`: numpy array (height, width, 3) in BGR format or None

#### def `isOpened`()
> **RETURNS**
> * True if `source` is opened, False otherwise.

#### def `close`()
> Closes `source`

#### def `get`( prop_id )
> **ARGS**
> * (int) `prop_id`: Property identifier (use CAP_PROP_* constants)
>
> **RETURNS**
> * float: Property value, or -1 if not supported/available
>
> **CAP_PROP_\***:
>
> ```
> 0 = CAP_PROP_POS_MSEC
> 1 = CAP_PROP_POS_FRAMES
> 2 = CAP_PROP_POS_AVI_RATIO
> 3 = CAP_PROP_FRAME_WIDTH
> 4 = CAP_PROP_FRAME_HEIGHT
> 5 = CAP_PROP_FPS
> 6 = CAP_PROP_FOURCC
> 7 = CAP_PROP_FRAME_COUNT
> ```

## Example Usage

#### No resizing, explicit open + checks
```python
import degirum_video_capture as dvc

cap = dvc.VideoCapture()
cap.open("example.mp4")

if cap.isOpened():
    while True:
        ret, frame = capture.read()
        if not ret:
            break
        # Do something with the frame
        foo_bar(frame)

    cap.close()
```

#### Resizing + letterboxing, implicit open and no checks, used as context manager
```python
import degirum_video_capture as dvc

with dvc.VideoCapture("example.mp4", 640, 640) as capture:
    while True:
        ret, frame = capture.read()
        if not ret:
            break
        # Do something with the frame
        foo_bar(frame)
```