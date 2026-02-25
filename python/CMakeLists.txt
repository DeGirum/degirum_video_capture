cmake_minimum_required(VERSION 3.16)

# Find pybind11
find_package(pybind11 REQUIRED CONFIG PATHS ${pybind11_INSTALL_DIR} NO_DEFAULT_PATH)

# Configure the C++ bindings file with version from main project
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/video_capture_bindings.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/video_capture_bindings_configured.cpp
    @ONLY
)

# Python bindings using pybind11
pybind11_add_module(_video_capture NO_EXTRAS ${CMAKE_CURRENT_BINARY_DIR}/video_capture_bindings_configured.cpp)

# Link video_capture library
target_link_libraries(_video_capture PRIVATE video_capture)

# Hide all symbols except Python module entry point to avoid conflicts with OpenCV's FFmpeg
if(UNIX AND NOT APPLE)
    target_link_options(_video_capture PRIVATE 
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/symbol_version.map
    )
endif()

target_include_directories(_video_capture PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(_video_capture PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    FOLDER "Python"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/package/degirum_video_capture"
)

# Copy Python package files to build directory
set(PYTHON_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")

# Generate _version.py with current project version (build directory only)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/_version.py.in
    ${PYTHON_PACKAGE_DIR}/degirum_video_capture/_version.py
    @ONLY
)

# Copy __init__.py and other package files
add_custom_command(
    TARGET _video_capture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/degirum_video_capture
        ${PYTHON_PACKAGE_DIR}/degirum_video_capture
    DEPENDS ${PYTHON_PACKAGE_DIR}/degirum_video_capture/_version.py
    COMMENT "Copying Python package files"
)

# Copy setup.py and README
add_custom_command(
    TARGET _video_capture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/setup.py
        ${PYTHON_PACKAGE_DIR}/setup.py
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/README.md
        ${PYTHON_PACKAGE_DIR}/README.md
    COMMENT "Copying setup.py and README.md"
)

# Build wheel
add_custom_target(wheel ALL
    COMMAND ${Python3_EXECUTABLE} -m pip wheel --no-deps -w ${CMAKE_BINARY_DIR}/wheels ${PYTHON_PACKAGE_DIR}
    DEPENDS _video_capture
    WORKING_DIRECTORY ${PYTHON_PACKAGE_DIR}
    COMMENT "Building Python wheel package"
)

# Print wheel location after build
add_custom_command(TARGET wheel POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -c "import glob; files = glob.glob('${CMAKE_BINARY_DIR}/wheels/*.whl'); print('Wheel written to ' + (files[0] if files else '${CMAKE_BINARY_DIR}/wheels/'))"
    VERBATIM
)
message(STATUS "To test without installing, run: PYTHONPATH=${PYTHON_PACKAGE_DIR} python3 <script.py>")

# Install the Python module (optional, for those who prefer cmake install)
find_package(Python3 COMPONENTS Interpreter Development)
if(DEFINED Python3_SITEARCH)
    message(STATUS "Python interpreter found. _video_capture install target configured to ${Python3_SITEARCH}/degirum_video_capture")
    install(
        TARGETS _video_capture
        COMPONENT python
        DESTINATION "${Python3_SITEARCH}/degirum_video_capture"
    )
else()
    message(STATUS "Python interpreter not found. _video_capture install target not configured.")
endif()

