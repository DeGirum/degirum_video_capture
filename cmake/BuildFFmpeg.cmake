# ##############################################################################
# BuildFFmpeg.cmake
# ##############################################################################
#
# Custom build function for FFmpeg as static libraries
#

function(BuildFFmpeg SOURCE_DIR BUILD_DIR INSTALL_DIR)
  message(STATUS "Building FFmpeg as static libraries")
  
  string(SHA256 BUILD_HASH "${SOURCE_DIR}${BUILD_DIR}${INSTALL_DIR}")
  set(CHECKPOINT_NAME ${BUILD_DIR}/ffmpeg_build_${BUILD_HASH})
  
  if(EXISTS ${CHECKPOINT_NAME})
    message(STATUS "FFmpeg already built (checkpoint exists)")
    
    # Create imported library targets
    add_library(FFmpeg::avcodec STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::avformat STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::avutil STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::avfilter STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::swscale STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::swresample STATIC IMPORTED GLOBAL)
    
    set_target_properties(FFmpeg::avcodec PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavcodec.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::avformat PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavformat.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::avutil PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavutil.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::avfilter PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavfilter.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::swscale PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libswscale.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::swresample PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libswresample.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    
    set(FFMPEG_INCLUDE_DIRS ${INSTALL_DIR}/include PARENT_SCOPE)
    set(FFMPEG_LIBRARIES 
      FFmpeg::avformat
      FFmpeg::avcodec
      FFmpeg::avfilter
      FFmpeg::swscale
      FFmpeg::swresample
      FFmpeg::avutil
      PARENT_SCOPE
    )
    
    return()
  endif()
  
  # Check if already built
  if(EXISTS ${INSTALL_DIR}/lib/libavcodec.a AND 
     EXISTS ${INSTALL_DIR}/lib/libavformat.a AND
     EXISTS ${INSTALL_DIR}/lib/libavutil.a AND
     EXISTS ${INSTALL_DIR}/lib/libavfilter.a AND
     EXISTS ${INSTALL_DIR}/lib/libswscale.a)
    message(STATUS "FFmpeg libraries already exist, skipping build")
    file(TOUCH ${CHECKPOINT_NAME})
    
    # Create imported library targets
    add_library(FFmpeg::avcodec STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::avformat STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::avutil STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::avfilter STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::swscale STATIC IMPORTED GLOBAL)
    add_library(FFmpeg::swresample STATIC IMPORTED GLOBAL)
    
    set_target_properties(FFmpeg::avcodec PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavcodec.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::avformat PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavformat.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::avutil PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavutil.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::avfilter PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavfilter.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::swscale PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libswscale.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    set_target_properties(FFmpeg::swresample PROPERTIES
      IMPORTED_LOCATION ${INSTALL_DIR}/lib/libswresample.a
      INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
    )
    
    set(FFMPEG_INCLUDE_DIRS ${INSTALL_DIR}/include PARENT_SCOPE)
    set(FFMPEG_LIBRARIES 
      FFmpeg::avformat
      FFmpeg::avcodec
      FFmpeg::avfilter
      FFmpeg::swscale
      FFmpeg::swresample
      FFmpeg::avutil
      PARENT_SCOPE
    )
    
    return()
  endif()
  
  # Create build directory
  file(MAKE_DIRECTORY ${BUILD_DIR})
  
  # Determine the number of parallel jobs
  include(ProcessorCount)
  ProcessorCount(N_JOBS)
  if(N_JOBS EQUAL 0)
    set(N_JOBS 4)
  endif()
  
  # Detect CPU architecture and set SIMD optimizations
  set(SIMD_FLAGS)
  set(MARCH_FLAGS "")
  
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    message(STATUS "Detected x86_64 architecture - enabling PIC-compatible assembly")
    message(STATUS "Note: Building FFmpeg with assembly enabled and PIC for static linking")
    # Enable assembly with PIC - FFmpeg can generate position-independent assembly
    list(APPEND SIMD_FLAGS --enable-asm)
    set(MARCH_FLAGS "-march=native")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    message(STATUS "Detected ARM64 architecture - enabling NEON optimizations")
    list(APPEND SIMD_FLAGS --enable-neon)
    set(MARCH_FLAGS "-march=native")
  else()
    message(STATUS "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR} - using default optimizations")
    set(MARCH_FLAGS "-march=native")
  endif()
  
  # Build comprehensive optimization flags
  # Important: -fPIC must come FIRST to ensure it's applied to all compilation units
  # Use x86-64-v3 for broad compatibility with modern CPUs (2015+) while keeping AVX2/BMI2/F16C optimizations
  set(OPT_CFLAGS "-fPIC -O3 -march=x86-64-v3 -mtune=generic -ffast-math -funroll-loops")
  set(OPT_CXXFLAGS "-fPIC -O3 -march=x86-64-v3 -mtune=generic -ffast-math -funroll-loops")
  set(OPT_LDFLAGS "-fPIC")
  
  message(STATUS "Using CFLAGS: ${OPT_CFLAGS}")
  message(STATUS "Using SIMD flags: ${SIMD_FLAGS}")
  
  # Platform-specific configuration and hardware acceleration
  set(HW_ACCEL_FLAGS)
  
  if(APPLE)
    message(STATUS "macOS detected - enabling VideoToolbox hardware acceleration")
    list(APPEND HW_ACCEL_FLAGS --enable-videotoolbox)
  elseif(UNIX)
    # Check for CUDA
    if(EXISTS "/usr/local/cuda/include/cuda.h")
      message(STATUS "CUDA detected - enabling NVIDIA hardware acceleration")
      list(APPEND HW_ACCEL_FLAGS 
        --enable-cuda 
        --enable-cuda-nvcc 
        --enable-cuvid 
        --enable-nvenc 
        --enable-nonfree
      )
      # Add CUDA paths to CFLAGS/LDFLAGS
      set(OPT_CFLAGS "${OPT_CFLAGS} -I/usr/local/cuda/include")
      set(CUDA_LDFLAGS "-L/usr/local/cuda/lib64")
    else()
      message(STATUS "No CUDA detected - skipping hardware acceleration")
      # Note: VAAPI requires libva-dev which may not be installed
      # Skipping VAAPI to avoid build failures
    endif()
  endif()
  
  # Threading optimizations
  set(THREADING_FLAGS --enable-pthreads)
  
  # Configure FFmpeg with all optimizations
  message(STATUS "Configuring FFmpeg with optimizations enabled...")
  
  # Build configure command as a list
  set(CONFIGURE_CMD
    ${SOURCE_DIR}/configure
    --prefix=${INSTALL_DIR}
    --disable-shared
    --enable-static
    --enable-pic
    --extra-cflags=${OPT_CFLAGS}
    --extra-cxxflags=${OPT_CXXFLAGS}
    --extra-ldflags=${OPT_LDFLAGS}
    ${SIMD_FLAGS}
    ${THREADING_FLAGS}
    ${HW_ACCEL_FLAGS}
    --disable-programs
    --disable-doc
    --disable-htmlpages
    --disable-manpages
    --disable-podpages
    --disable-txtpages
    --enable-avcodec
    --enable-avformat
    --enable-avutil
    --enable-avfilter
    --enable-swscale
    --enable-swresample
  )
  
  # Add CUDA linker flags if present
  if(DEFINED CUDA_LDFLAGS)
    list(APPEND CONFIGURE_CMD --extra-ldflags=${CUDA_LDFLAGS})
  endif()
  
  execute_process(
    COMMAND ${CONFIGURE_CMD}
    WORKING_DIRECTORY ${BUILD_DIR}
    RESULT_VARIABLE CONFIGURE_RESULT
    OUTPUT_FILE ${BUILD_DIR}/configure_output.log
    ERROR_FILE ${BUILD_DIR}/configure_error.log
  )
  
  if(NOT CONFIGURE_RESULT EQUAL 0)
    file(READ ${BUILD_DIR}/configure_error.log CONFIGURE_ERROR)
    file(READ ${BUILD_DIR}/configure_output.log CONFIGURE_OUTPUT)
    message(FATAL_ERROR "FFmpeg configure failed with exit code ${CONFIGURE_RESULT}\n"
                        "Error log: ${CONFIGURE_ERROR}\n"
                        "Output log: ${CONFIGURE_OUTPUT}\n"
                        "Check ${BUILD_DIR}/config.log for more details")
  endif()
  
  message(STATUS "Building FFmpeg with ${N_JOBS} parallel jobs...")
  execute_process(
    COMMAND make -j${N_JOBS}
    WORKING_DIRECTORY ${BUILD_DIR}
    RESULT_VARIABLE BUILD_RESULT
    OUTPUT_VARIABLE BUILD_OUTPUT
    ERROR_VARIABLE BUILD_ERROR
  )
  
  if(NOT BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "FFmpeg build failed: ${BUILD_ERROR}")
  endif()
  
  message(STATUS "Installing FFmpeg...")
  execute_process(
    COMMAND make install
    WORKING_DIRECTORY ${BUILD_DIR}
    RESULT_VARIABLE INSTALL_RESULT
    OUTPUT_VARIABLE INSTALL_OUTPUT
    ERROR_VARIABLE INSTALL_ERROR
  )
  
  if(NOT INSTALL_RESULT EQUAL 0)
    message(FATAL_ERROR "FFmpeg install failed: ${INSTALL_ERROR}")
  endif()
  
  # Create checkpoint
  file(TOUCH ${CHECKPOINT_NAME})
  
  message(STATUS "FFmpeg built successfully")
  
  # Create imported library targets
  add_library(FFmpeg::avcodec STATIC IMPORTED GLOBAL)
  add_library(FFmpeg::avformat STATIC IMPORTED GLOBAL)
  add_library(FFmpeg::avutil STATIC IMPORTED GLOBAL)
  add_library(FFmpeg::avfilter STATIC IMPORTED GLOBAL)
  add_library(FFmpeg::swscale STATIC IMPORTED GLOBAL)
  add_library(FFmpeg::swresample STATIC IMPORTED GLOBAL)
  
  set_target_properties(FFmpeg::avcodec PROPERTIES
    IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavcodec.a
    INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
  )
  set_target_properties(FFmpeg::avformat PROPERTIES
    IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavformat.a
    INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
  )
  set_target_properties(FFmpeg::avutil PROPERTIES
    IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavutil.a
    INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
  )
  set_target_properties(FFmpeg::avfilter PROPERTIES
    IMPORTED_LOCATION ${INSTALL_DIR}/lib/libavfilter.a
    INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
  )
  set_target_properties(FFmpeg::swscale PROPERTIES
    IMPORTED_LOCATION ${INSTALL_DIR}/lib/libswscale.a
    INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
  )
  set_target_properties(FFmpeg::swresample PROPERTIES
    IMPORTED_LOCATION ${INSTALL_DIR}/lib/libswresample.a
    INTERFACE_INCLUDE_DIRECTORIES ${INSTALL_DIR}/include
  )
  
  set(FFMPEG_INCLUDE_DIRS ${INSTALL_DIR}/include PARENT_SCOPE)
  set(FFMPEG_LIBRARIES 
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avfilter
    FFmpeg::swscale
    FFmpeg::swresample
    FFmpeg::avutil
    PARENT_SCOPE
  )
  
endfunction()

