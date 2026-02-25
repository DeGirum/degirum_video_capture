# ##############################################################################
# BuildFFmpeg.cmake
# ##############################################################################
#
# Custom build function for FFmpeg as static libraries
#

function(BuildFFmpeg SOURCE_DIR BUILD_DIR INSTALL_DIR)
  # Verify we are building on Linux only
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This FFmpeg build configuration is Linux-only. Detected system: ${CMAKE_SYSTEM_NAME}")
  endif()
  
  message(STATUS "Building FFmpeg as static libraries")
  
  # Determine the number of parallel jobs
  include(ProcessorCount)
  ProcessorCount(N_JOBS)
  if(N_JOBS EQUAL 0)
    set(N_JOBS 4)
  endif()
  
  # Detect CPU architecture and set SIMD optimizations
  set(SIMD_FLAGS)
  set(MARCH_FLAGS "")
  set(MTUNE_FLAGS "")
  
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    # Check if nasm or yasm is available for x86_64 assembly
    find_program(NASM_FOUND nasm)
    find_program(YASM_FOUND yasm)
    
    if(NASM_FOUND OR YASM_FOUND)
      message(STATUS "Detected x86_64 architecture - enabling PIC-compatible assembly")
      if(NASM_FOUND)
        message(STATUS "Using nasm for assembly: ${NASM_FOUND}")
      else()
        message(STATUS "Using yasm for assembly: ${YASM_FOUND}")
      endif()
      list(APPEND SIMD_FLAGS --enable-asm)
    else()
      message(FATAL_ERROR "nasm or yasm not found - required for x86_64 FFmpeg build\n"
                          "Install with: apt-get install nasm (Ubuntu/Debian) or yum install nasm (RedHat/CentOS)")
    endif()
    
    # Use x86-64 baseline for maximum compatibility
    # Individual CPU features (SSE, AVX, etc.) are detected at runtime by FFmpeg
    set(MARCH_FLAGS "-march=x86-64")
    set(MTUNE_FLAGS "-mtune=generic")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    message(STATUS "Detected ARM64 architecture - enabling NEON optimizations")
    list(APPEND SIMD_FLAGS --enable-neon)
    
    # For ARM64, use armv8-a baseline
    set(MARCH_FLAGS "-march=armv8-a")
    set(MTUNE_FLAGS "")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686|i386")
    message(STATUS "Detected i686/x86 architecture")
    set(MARCH_FLAGS "-march=i686")
    set(MTUNE_FLAGS "-mtune=generic")
  else()
    message(STATUS "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR} - using default optimizations")
    set(MARCH_FLAGS "")
    set(MTUNE_FLAGS "")
  endif()
  
  # Build comprehensive optimization flags
  # Important: -fPIC must come FIRST to ensure it's applied to all compilation units
  set(OPT_CFLAGS "-fPIC -O3 ${MARCH_FLAGS} ${MTUNE_FLAGS} -ffast-math -funroll-loops")
  set(OPT_CXXFLAGS "-fPIC -O3 ${MARCH_FLAGS} ${MTUNE_FLAGS} -ffast-math -funroll-loops")
  set(OPT_LDFLAGS "-fPIC")
  
  message(STATUS "Using CFLAGS: ${OPT_CFLAGS}")
  message(STATUS "Using SIMD flags: ${SIMD_FLAGS}")
  
  # Platform-specific configuration and hardware acceleration
  set(HW_ACCEL_FLAGS)
  
  message(STATUS "Linux detected - using CPU decoding only")
  # Note: Hardware acceleration disabled for maximum compatibility
  # VAAPI support requires libva-dev which may not be installed
  # CPU decoding is sufficient for most use cases
  
  # Threading optimizations
  set(THREADING_FLAGS --enable-pthreads)
  
  # Create hash from all parameters that affect the build
  # This ensures rebuild when configuration changes
  string(SHA256 CONFIG_HASH "${SOURCE_DIR};${CMAKE_C_COMPILER};${CMAKE_CXX_COMPILER};${CMAKE_BUILD_TYPE};${CMAKE_SYSTEM_PROCESSOR};${OPT_CFLAGS};${SIMD_FLAGS}")
  set(CHECKPOINT_NAME ${BUILD_DIR}/ffmpeg_build_${CONFIG_HASH})
  
  # Check if already built with this exact configuration
  if(EXISTS ${CHECKPOINT_NAME} AND
     EXISTS ${INSTALL_DIR}/lib/libavcodec.a AND 
     EXISTS ${INSTALL_DIR}/lib/libavformat.a AND
     EXISTS ${INSTALL_DIR}/lib/libavutil.a AND
     EXISTS ${INSTALL_DIR}/lib/libavfilter.a AND
     EXISTS ${INSTALL_DIR}/lib/libswscale.a AND
     EXISTS ${INSTALL_DIR}/lib/libswresample.a)
    message(STATUS "FFmpeg already built with current configuration (checkpoint: ${CONFIG_HASH})")
    
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
  
  # Need to build - remove old checkpoints first
  file(GLOB OLD_CHECKPOINTS "${BUILD_DIR}/ffmpeg_build_*")
  if(OLD_CHECKPOINTS)
    file(REMOVE ${OLD_CHECKPOINTS})
    message(STATUS "Removed old FFmpeg build checkpoints")
  endif()
  
  # Check if libraries exist from previous build but with different config
  if(EXISTS ${INSTALL_DIR}/lib/libavcodec.a AND 
     EXISTS ${INSTALL_DIR}/lib/libavformat.a AND
     EXISTS ${INSTALL_DIR}/lib/libavutil.a AND
     EXISTS ${INSTALL_DIR}/lib/libavfilter.a AND
     EXISTS ${INSTALL_DIR}/lib/libswscale.a AND
     EXISTS ${INSTALL_DIR}/lib/libswresample.a)
    message(STATUS "FFmpeg libraries exist but configuration changed, will rebuild")
  endif()
  
  # Create build directory
  file(MAKE_DIRECTORY ${BUILD_DIR})
  
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

