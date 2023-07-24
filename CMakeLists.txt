cmake_minimum_required(VERSION 3.16)
project(PDFLA)

# Setup the Targoman CMake build system
include(./3rdParty/TargomanCMake/targoman.cmake)
tg_setup_buildsystem_paths()

# Global compiler settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3rdParty folder (Even though the libraries are not actually 3rd party :) )
add_subdirectory(3rdParty)

# OpenCV
if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Emscripten")
    find_package(OpenCV REQUIRED)
endif()

# Main library
tg_add_library(pdfla
    STATIC
    libsrc/pdfla.cpp
    libsrc/clsPdfiumWrapper.cpp
    libsrc/dla.cpp
    libsrc/debug.cpp
)

tg_add_library_headers(pdfla
    PUBLIC_HEADER
    libsrc/pdfla.h
    libsrc/dla.h
    libsrc/type_traits.hpp
)

tg_add_library_headers(pdfla
    PRIVATE_HEADER
    libsrc/debug.h
)

target_include_directories(pdfla
    PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

# Tests
if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Emscripten")
add_executable(test_PDFLA
    tests/blackboxTest.cpp
)

target_link_directories(test_PDFLA
    PRIVATE
    ${OpenCV_LIB_DIRS}
)
target_link_libraries(test_PDFLA
    pdfla
    ${OpenCV_LIBS}
    fpdfapi
    fdrm
    fpdfdoc
    fpdftext
    fxcodec
    fxcrt
    fxge    
)
endif()

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Emscripten")
add_executable(PDFLAJS
    wasm/interface.cpp
)
target_link_libraries(PDFLAJS
    pdfla
    fpdfapi
    fdrm
    fpdfdoc
    fpdftext
    fxcodec
    fxcrt
    fxge    
)
target_compile_options(PDFLAJS
    PUBLIC
    -flto
    -fno-exceptions
)
target_link_options(PDFLAJS
    PUBLIC
    -Oz
    -sINVOKE_RUN=0
    -sASSERTIONS=1
    -sELIMINATE_DUPLICATE_FUNCTIONS=1
    -sFILESYSTEM=0
    -sMODULARIZE=1
    -flto
    --bind
    --no-entry
)
endif()

# Finalize the settings
tg_process_all_targets()
