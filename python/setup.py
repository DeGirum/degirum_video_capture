"""
Setup script for degirum_video_capture package
"""

from setuptools import setup, find_packages

setup(
    name="degirum_video_capture",
    version="1.0.0",
    author="DeGirum Corporation",
    author_email="support@degirum.com",
    description="High-performance video capture library using FFmpeg",
    long_description=open("README.md").read() if __file__ else "",
    long_description_content_type="text/markdown",
    url="https://github.com/DeGirum/degirum_video_capture",
    packages=find_packages(),
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Multimedia :: Video",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
    ],
    python_requires=">=3.8",
    install_requires=[
        "numpy>=1.19.0",
    ],
    package_data={
        "degirum_video_capture": ["*.so", "*.pyd", "*.dll"],
    },
    include_package_data=True,
)

