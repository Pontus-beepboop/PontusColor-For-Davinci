# ============================================================================
#  PontusColor - OpenFX plugin for DaVinci Resolve 21 (Color page)
#  Cross-platform build:  macOS (Metal, universal) / Windows (OpenCL [+CUDA])
# ============================================================================
cmake_minimum_required(VERSION 3.18)
project(PontusColor LANGUAGES CXX)

option(PONTUS_WITH_CUDA "Also build the CUDA backend (needs CUDA Toolkit)" OFF)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(OFX_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/openfx)
set(SRC      ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
#  OpenFX C++ Support library (vendored)
# ---------------------------------------------------------------------------
file(GLOB SUPPORT_SRC ${OFX_ROOT}/Support/Library/ofxs*.cpp)
add_library(ofxsupport STATIC ${SUPPORT_SRC})
target_include_directories(ofxsupport PUBLIC
    ${OFX_ROOT}/include ${OFX_ROOT}/Support/include)
target_compile_definitions(ofxsupport PUBLIC OFX_EXTENSIONS_RESOLVE OFX_EXTENSIONS_NUKE)
set_target_properties(ofxsupport PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
#  Embed the shared color engine as a runtime-compiled source string for the
#  OpenCL and Metal backends (header text + the relevant entry kernel).
#  Emitted as a byte array (not a string literal) so it has no length limit -
#  MSVC caps a single string literal well below our ~22 KB of source.
# ---------------------------------------------------------------------------
file(READ ${SRC}/PontusColorKernel.h KERNEL_HEX HEX)

# helper: turn a concatenated hex blob into "0xAB,0xCD,..." then a C array
function(write_embedded_source out_file symbol hex_blob)
    string(REGEX REPLACE "(..)" "0x\\1," _bytes "${hex_blob}")
    file(WRITE ${out_file}
"extern const char* ${symbol};\n\
static const unsigned char _data[] = { ${_bytes} 0x00 };\n\
const char* ${symbol} = (const char*)_data;\n")
endfunction()

file(READ ${SRC}/PontusColorOpenCLEntry.cl OCL_HEX HEX)
set(GEN_OCL ${CMAKE_BINARY_DIR}/PontusColorKernelSource.cpp)
# 0a = newline joining the header and the entry kernel
write_embedded_source(${GEN_OCL} PontusColorKernelSource "${KERNEL_HEX}0a${OCL_HEX}")

file(READ ${SRC}/PontusColorMetalEntry.metal MTL_HEX HEX)
set(GEN_MTL ${CMAKE_BINARY_DIR}/PontusColorKernelSourceMetal.cpp)
write_embedded_source(${GEN_MTL} PontusColorKernelSourceMetal "${KERNEL_HEX}0a${MTL_HEX}")

# ---------------------------------------------------------------------------
#  The plugin module (output: PontusColor.ofx)
# ---------------------------------------------------------------------------
set(PLUGIN_SOURCES ${SRC}/PontusColorPlugin.cpp)

if(APPLE)
    enable_language(OBJCXX)
    add_library(PontusColor MODULE
        ${PLUGIN_SOURCES}
        ${SRC}/PontusColorMetal.mm
        ${GEN_MTL})
    target_link_libraries(PontusColor PRIVATE ofxsupport
        "-framework Metal" "-framework Foundation")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set_target_properties(PontusColor PROPERTIES OSX_ARCHITECTURES "x86_64;arm64")
    endif()
else()
    set(BACKEND_SOURCES ${SRC}/PontusColorOpenCL.cpp ${GEN_OCL})
    if(PONTUS_WITH_CUDA)
        enable_language(CUDA)
        list(APPEND BACKEND_SOURCES ${SRC}/PontusColorCUDA.cu)
    endif()

    add_library(PontusColor MODULE ${PLUGIN_SOURCES} ${BACKEND_SOURCES})

    find_package(OpenCL REQUIRED)
    target_link_libraries(PontusColor PRIVATE ofxsupport OpenCL::OpenCL)

    if(PONTUS_WITH_CUDA)
        find_package(CUDAToolkit REQUIRED)
        target_compile_definitions(PontusColor PRIVATE PONTUS_WITH_CUDA)
        target_link_libraries(PontusColor PRIVATE CUDA::cudart)
        set_target_properties(PontusColor PROPERTIES
            CUDA_SEPARABLE_COMPILATION ON
            CUDA_ARCHITECTURES "all-major")
    endif()

    if(NOT WIN32)
        find_package(Threads REQUIRED)
        target_link_libraries(PontusColor PRIVATE Threads::Threads)
    endif()
endif()

target_include_directories(PontusColor PRIVATE
    ${SRC} ${OFX_ROOT}/include ${OFX_ROOT}/Support/include)
target_compile_definitions(PontusColor PRIVATE OFX_EXTENSIONS_RESOLVE OFX_EXTENSIONS_NUKE)
set_target_properties(PontusColor PROPERTIES PREFIX "" OUTPUT_NAME "PontusColor" SUFFIX ".ofx")

# ---------------------------------------------------------------------------
#  Assemble the .ofx.bundle next to the build
# ---------------------------------------------------------------------------
set(BUNDLE ${CMAKE_BINARY_DIR}/PontusColor.ofx.bundle)
if(APPLE)
    set(ARCHDIR ${BUNDLE}/Contents/MacOS)
elseif(WIN32)
    set(ARCHDIR ${BUNDLE}/Contents/Win64)
else()
    set(ARCHDIR ${BUNDLE}/Contents/Linux-x86-64)
endif()

add_custom_command(TARGET PontusColor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ARCHDIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:PontusColor> ${ARCHDIR}/PontusColor.ofx
    COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE}/Contents
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist ${BUNDLE}/Contents/Info.plist
    COMMENT "Assembling PontusColor.ofx.bundle")

message(STATUS "PontusColor will be bundled at: ${BUNDLE}")
