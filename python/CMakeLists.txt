cmake_minimum_required(VERSION 3.16)

# Find pybind11
find_package(pybind11 REQUIRED CONFIG PATHS ${pybind11_INSTALL_DIR} NO_DEFAULT_PATH)

# Python bindings using pybind11
pybind11_add_module(_video_capture NO_EXTRAS video_capture_bindings.cpp)

target_link_libraries(_video_capture PRIVATE video_capture)

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

# Copy __init__.py
add_custom_command(
    TARGET _video_capture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/degirum_video_capture
        ${PYTHON_PACKAGE_DIR}/degirum_video_capture
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

message(STATUS "Wheel will be created in: ${CMAKE_BINARY_DIR}/wheels")
message(STATUS "To test without installing, run: PYTHONPATH=${PYTHON_PACKAGE_DIR} python3 <script.py>")

# Add test target (run test_video_capture.py from build without installing)
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/test_video_capture.py)
    add_custom_target(test_module
        COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${PYTHON_PACKAGE_DIR}
                ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_video_capture.py
        DEPENDS _video_capture
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running test_video_capture.py without installation"
    )
    
    # Automatic test execution logic
    set(TEST_STAMP_FILE "${CMAKE_BINARY_DIR}/.tests_completed")
    
    if(RUN_TESTS)
        # User explicitly enabled tests - always run them
        add_custom_command(TARGET wheel POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${PYTHON_PACKAGE_DIR}
                    ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_video_capture.py
            COMMAND ${CMAKE_COMMAND} -E touch ${TEST_STAMP_FILE}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running tests (forced with -DRUN_TESTS=ON)"
        )
        message(STATUS "Tests will run on every build (forced with -DRUN_TESTS=ON)")
    else()
        # Default behavior: run tests once on first build only
        if(NOT EXISTS ${TEST_STAMP_FILE})
            add_custom_command(TARGET wheel POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${PYTHON_PACKAGE_DIR}
                        ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_video_capture.py
                COMMAND ${CMAKE_COMMAND} -E touch ${TEST_STAMP_FILE}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                COMMENT "Running tests (first build only)"
            )
            message(STATUS "Tests will run on first build only")
        else()
            add_custom_command(TARGET wheel POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo ""
                COMMAND ${CMAKE_COMMAND} -E echo "============================================================"
                COMMAND ${CMAKE_COMMAND} -E echo "Tests skipped (already completed on first build)"
                COMMAND ${CMAKE_COMMAND} -E echo "To re-run tests: cmake .. -DRUN_TESTS=ON && cmake --build ."
                COMMAND ${CMAKE_COMMAND} -E echo "Or clean build: rm -rf build && mkdir build && cmake .."
                COMMAND ${CMAKE_COMMAND} -E echo "============================================================"
                COMMAND ${CMAKE_COMMAND} -E echo ""
                COMMENT ""
            )
            message(STATUS "Tests already completed. Use -DRUN_TESTS=ON to re-run.")
        endif()
    endif()
endif()

# Print wheel location after build
add_custom_command(TARGET wheel POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -c "import glob; files = glob.glob('${CMAKE_BINARY_DIR}/wheels/*.whl'); print('Wheel written to ' + (files[0] if files else '${CMAKE_BINARY_DIR}/wheels/'))"
    VERBATIM
)

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

