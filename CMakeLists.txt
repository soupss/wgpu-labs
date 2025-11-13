cmake_minimum_required(VERSION 3.0...3.25)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(core LANGUAGES C CXX)

file(GLOB SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
add_executable(bin ${SOURCES})

set_target_properties(bin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)
 
# Figure out Homebrew prefix on macOS
if(APPLE AND EXISTS /opt/homebrew)
  set(HOMEBREW_PREFIX /opt/homebrew)   # Apple Silicon
elseif(APPLE)
  set(HOMEBREW_PREFIX /usr/local)      # Intel
endif()

# Find headers (webgpu.h) and the wgpu-native library
find_path(WEBGPU_INCLUDE_DIR
  NAMES webgpu/webgpu.h webgpu.h
  PATHS
    ${HOMEBREW_PREFIX}/include
    ${HOMEBREW_PREFIX}/opt/wgpu-native/include
  PATH_SUFFIXES webgpu headers
)

find_library(WGPU_NATIVE_LIB
  NAMES wgpu_native
  PATHS
    ${HOMEBREW_PREFIX}/lib
    ${HOMEBREW_PREFIX}/opt/wgpu-native/lib
)

find_package(SDL3 REQUIRED CONFIG)

if(NOT WEBGPU_INCLUDE_DIR OR NOT WGPU_NATIVE_LIB)
  message(FATAL_ERROR "Could not find webgpu headers or wgpu_native library")
endif()

target_include_directories(bin PRIVATE ${WEBGPU_INCLUDE_DIR})
target_link_libraries(bin PRIVATE ${WGPU_NATIVE_LIB} SDL3::SDL3)

# On macOS you’ll usually also need system frameworks for window/surface integration
if(APPLE)
    target_link_options(bin PRIVATE
        "-Wl,-framework,Metal"
        "-Wl,-framework,QuartzCore"
        "-Wl,-framework,Cocoa"
    )
    # Make sure the app can find the dylib at runtime if you use the shared lib
    set_target_properties(bin PROPERTIES BUILD_RPATH "${HOMEBREW_PREFIX}/lib")
endif()

