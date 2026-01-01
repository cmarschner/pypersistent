"""
Setup script for persistent_map_cpp extension module.

Build with: python setup.py build_ext --inplace
Install with: python setup.py install
Create wheel: python setup.py bdist_wheel
"""

from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext
import sys
import platform

# Determine optimization flags based on platform
extra_compile_args = []
extra_link_args = []

if platform.system() == "Windows":
    # MSVC compiler flags
    extra_compile_args = [
        "/O2",          # Maximum optimization
        "/std:c++17",   # C++17 standard
        "/DNDEBUG",     # Disable assertions
    ]
else:
    # GCC/Clang flags (Linux, macOS)
    extra_compile_args = [
        "-O3",              # Maximum optimization
        "-march=native",    # Use native CPU instructions (POPCNT, etc.)
        "-std=c++17",       # C++17 standard
        "-ffast-math",      # Aggressive math optimizations
        "-DNDEBUG",         # Disable assertions in release
        "-Wall",            # Enable warnings
        "-Wextra",          # Extra warnings
    ]

# Define the extension module
ext_modules = [
    Pybind11Extension(
        "persistent_map_cpp",
        sources=[
            "src/persistent_map.cpp",
            "src/bindings.cpp",
        ],
        include_dirs=["src"],
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args,
        language="c++",
    ),
]

# Read long description from README if it exists
long_description = """
High-Performance Persistent Hash Map in C++
============================================

A C++ implementation of a persistent (immutable) hash map data structure using
Hash Array Mapped Trie (HAMT), with Python bindings via pybind11.

Features:
- Immutability with structural sharing
- O(log32 n) complexity for all operations
- 5-7x faster than pure Python implementation
- Thread-safe due to immutability
- Drop-in replacement for the Python version

Example:
    from persistent_map_cpp import PersistentMap

    m1 = PersistentMap()
    m2 = m1.assoc('key', 'value')
    # m1 is still empty, m2 contains the association
"""

setup(
    name="persistent_map_cpp",
    version="1.0.0",
    author="Your Name",
    author_email="your.email@example.com",
    description="High-performance persistent hash map (HAMT) in C++",
    long_description=long_description,
    long_description_content_type="text/plain",
    url="https://github.com/yourusername/persistent_map_cpp",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    python_requires=">=3.7",
    install_requires=[
        "pybind11>=2.10.0",
    ],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Topic :: Software Development :: Libraries",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: C++",
    ],
    zip_safe=False,
)
