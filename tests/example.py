"""
ORIGINAL PyAV Implementation (for reference)
This file shows the original PyAV-based implementation.
For the new implementation using degirum_video_capture, see example_usage.py
"""

import av
import av.error
import time

your_video = "/home/alexander-iakovlev/Workspaces/PySDKExamples/images/Traffic2.mp4"
your_video = "/home/alexander-iakovlev/Workspaces/degirum_video_capture/tests/SMPTE_Color_Bars_Large.mp4"

def srcq_pyav_fast_bgr(inp, w, h):
    container = av.open(inp)
    stream = container.streams.video[0]

    # Decode threading (CPU-only)
    stream.thread_type = "AUTO"
    # Optional tuning (benchmark):
    # stream.codec_context.thread_type = "FRAME"
    # stream.codec_context.thread_count = 4

    graph = av.filter.Graph()

    src = graph.add_buffer(template=stream)
    scale = graph.add("scale", f"{w}:{h}:force_original_aspect_ratio=decrease")
    pad   = graph.add("pad",   f"{w}:{h}:(ow-iw)/2:(oh-ih)/2")
    fmt   = graph.add("format", "bgr24")
    sink  = graph.add("buffersink")

    src.link_to(scale)
    scale.link_to(pad)
    pad.link_to(fmt)
    fmt.link_to(sink)

    graph.configure()

    try:
        for frame in container.decode(stream):
            src.push(frame)

            while True:
                try:
                    out = sink.pull()
                except (av.error.FFmpegError, BlockingIOError):
                    break

                # numpy.ndarray, uint8, (h, w, 3), BGR
                yield out.to_ndarray()
    finally:
        container.close()


# Example usage with PyAV
if __name__ == "__main__":
    n = 0
    start_time = time.time()

    for r in srcq_pyav_fast_bgr(your_video, 640, 640):
        if n == 10:
            start_time = time.time()
        n += 1
    
    elapsed_time = time.time() - start_time

    print(f"\nFPS: {n / elapsed_time:.2f}")
