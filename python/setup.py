"""
Setup script for degirum_video_capture package
"""

import os
from setuptools import setup, find_packages, Extension

# Get version from _version.py
def get_version():
    version_file = os.path.join(os.path.dirname(__file__), 'degirum_video_capture', '_version.py')
    
    if os.path.exists(version_file):
        version_globals = {}
        with open(version_file, 'r') as f:
            exec(f.read(), version_globals)
        return version_globals.get('__version__', '0.0.0')
    
    # Fallback: try to read from CMakeLists.txt if _version.py doesn't exist
    cmake_file = os.path.join(os.path.dirname(__file__), '..', 'CMakeLists.txt')
    if os.path.exists(cmake_file):
        try:
            with open(cmake_file, 'r') as f:
                for line in f:
                    if line.strip().startswith('project(') and 'VERSION' in line:
                        import re
                        match = re.search(r'VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)', line)
                        if match:
                            return match.group(1)
        except (IOError, OSError):
            pass
    
    return '0.0.0'  # Final fallback

setup(
    name="degirum_video_capture",
    version=get_version(),
    author="DeGirum Corporation",
    author_email="support@degirum.com",
    description="High-performance video capture library using FFmpeg",
    long_description=open(os.path.join(os.path.dirname(__file__), "..", "README.md")).read() if os.path.exists(os.path.join(os.path.dirname(__file__), "..", "README.md")) else "",
    long_description_content_type="text/markdown",
    url="https://github.com/DeGirum/degirum_video_capture",
    license="MIT",
    packages=find_packages(),
    ext_modules=[
        Extension(
            name='degirum_video_capture._video_capture',
            sources=[]  # Pre-built binary, no sources to compile
        )
    ],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Multimedia :: Video",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
    ],
    license="Proprietary (https://github.com/DeGirum/degirum_video_capture/blob/master/LICENSE)",
    python_requires=">=3.9",
    package_data={
        "degirum_video_capture": ["*.so", "*.pyd", "*.dll", "LICENSES/*"],
    },
    include_package_data=True,
)

