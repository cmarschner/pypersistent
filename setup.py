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
        "-std=c++17",       # C++17 standard
        "-ffast-math",      # Aggressive math optimizations
        "-DNDEBUG",         # Disable assertions in release
        "-Wall",            # Enable warnings
        "-Wextra",          # Extra warnings
    ]

    # Only add -march=native on Linux/x86_64 where it's safe
    # Avoid on macOS (M-series chips cause issues with clang)
    # Avoid on ARM (native detection is unreliable)
    if platform.system() == "Linux" and platform.machine() in ("x86_64", "AMD64"):
        extra_compile_args.append("-march=native")

# Define the extension module
ext_modules = [
    Pybind11Extension(
        "pypersistent",
        sources=[
            "src/persistent_dict.cpp",
            "src/persistent_array_map.cpp",
            "src/persistent_set.cpp",
            "src/persistent_list.cpp",
            "src/persistent_sorted_dict.cpp",
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
PyPersistent - High-Performance Persistent Hash Map
====================================================

A C++ implementation of a persistent (immutable) hash map data structure using
Hash Array Mapped Trie (HAMT), with Python bindings via pybind11.

Features:
- Immutability with structural sharing
- O(log32 n) complexity for all operations
- 38% faster than pure Python implementation
- 3270x faster than dict for structural sharing
- Thread-safe due to immutability
- Dual API: Functional and Pythonic

Example:
    from pypersistent import PersistentMap

    m1 = PersistentMap.create(a=1, b=2)
    m2 = m1.set('c', 3)  # Pythonic
    m3 = m1.assoc('c', 3)  # Functional
    # m1 is still {'a': 1, 'b': 2}
"""

setup(
    name="pypersistent",
    version="2.0.0b3",
    author="Clemens Marschner",
    author_email="mail@cmarschner.net",
    description="High-performance persistent data structures (HAMT, RB-Tree, Vector, Set) in C++",
    long_description=long_description,
    long_description_content_type="text/plain",
    url="https://github.com/cmarschner/pypersistent",
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
