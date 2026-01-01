cmake_minimum_required(VERSION 3.15)
project(persistent_map_cpp VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Python and pybind11
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Create Python extension module
pybind11_add_module(persistent_map_cpp
    src/persistent_map.cpp
    src/bindings.cpp
)

# Include directories
target_include_directories(persistent_map_cpp PRIVATE src)

# Optimization flags
if(MSVC)
    target_compile_options(persistent_map_cpp PRIVATE
        /O2
        /DNDEBUG
        /W4
    )
else()
    target_compile_options(persistent_map_cpp PRIVATE
        -O3
        -march=native
        -ffast-math
        -Wall
        -Wextra
        $<$<CONFIG:Release>:-DNDEBUG>
        $<$<CONFIG:Debug>:-g>
    )
endif()

# Set output directory
set_target_properties(persistent_map_cpp PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Installation rules
install(TARGETS persistent_map_cpp
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Testing (optional)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    # Add test configuration here
endif()

# Print configuration summary
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Python3 executable: ${Python3_EXECUTABLE}")
message(STATUS "Python3 include dirs: ${Python3_INCLUDE_DIRS}")
message(STATUS "pybind11 found: ${pybind11_FOUND}")
