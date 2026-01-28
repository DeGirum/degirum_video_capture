cmake_minimum_required(VERSION 3.16)

# Video Capture Library (C++)
add_library(video_capture STATIC
    VideoCapture.h
    VideoCapture.cpp
)

target_include_directories(video_capture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ffmpeg_INSTALL_DIR}/include
)

# Link FFmpeg libraries
target_link_libraries(video_capture PUBLIC
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avfilter
    FFmpeg::swscale
    FFmpeg::swresample
    FFmpeg::avutil
)

# Add system dependencies for FFmpeg
if(UNIX AND NOT APPLE)
    target_link_libraries(video_capture PUBLIC
        z        # zlib
        m        # math
        pthread  # threads
        dl       # dynamic linking
    )
    # Check for additional libraries that might be needed
    find_library(LZMA_LIBRARY lzma)
    if(LZMA_LIBRARY)
        target_link_libraries(video_capture PUBLIC ${LZMA_LIBRARY})
    endif()
    
    find_library(BZ2_LIBRARY bz2)
    if(BZ2_LIBRARY)
        target_link_libraries(video_capture PUBLIC ${BZ2_LIBRARY})
    endif()
    
elseif(APPLE)
    target_link_libraries(video_capture PUBLIC
        z
        m
        "-framework CoreFoundation"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework VideoToolbox"
    )
    find_library(LZMA_LIBRARY lzma)
    if(LZMA_LIBRARY)
        target_link_libraries(video_capture PUBLIC ${LZMA_LIBRARY})
    endif()
    
    find_library(BZ2_LIBRARY bz2)
    if(BZ2_LIBRARY)
        target_link_libraries(video_capture PUBLIC ${BZ2_LIBRARY})
    endif()
    
elseif(WIN32)
    target_link_libraries(video_capture PUBLIC
        ws2_32
        secur32
        bcrypt
    )
endif()

set_target_properties(video_capture PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

