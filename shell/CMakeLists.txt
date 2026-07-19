# Copyright (c) 2026 Open Cosmic Software. MIT licensed.
#
# OpenNyx browser shell — a real Chromium browser via CEF (Chromium Embedded
# Framework). This project downloads the official CEF Standard binary
# distribution at configure time (no binaries in the repo) and links a small
# C++ shell against it.
#
# Build (Windows x64, Visual Studio 2022):
#   cmake -S shell -B build -G "Visual Studio 17 2022" -A x64
#   cmake --build build --config Release --target opennyx

cmake_minimum_required(VERSION 3.21)

# Only generate Release configuration (plus Debug for multi-config IDEs).
set(CMAKE_CONFIGURATION_TYPES Debug Release)

project(opennyx LANGUAGES CXX)

# The Windows resource script (app icon + version info) needs the RC
# language.
if(WIN32)
  enable_language(RC)
endif()

set_property(GLOBAL PROPERTY OS_FOLDERS ON)

# Use folders in IDEs that support it.
set(CMAKE_FOLDER "opennyx")

#
# CEF configuration.
#

# Pinned CEF version (stable channel). Update deliberately; see
# https://cef-builds.spotifycdn.com/index.html for available builds.
set(CEF_VERSION "150.0.11+gb887805+chromium-150.0.7871.115")

# Determine the platform.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(CEF_PLATFORM "windows64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CEF_PLATFORM "linux64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(CEF_PLATFORM "macosarm64")
  else()
    set(CEF_PLATFORM "macosx64")
  endif()
else()
  message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# M1 runs without the Windows sandbox: the sandbox now requires the
# bootstrap.exe/DLL split which we adopt in M2. Must be set before
# find_package(CEF).
option(USE_SANDBOX "Enable the CEF sandbox" OFF)

# Download and extract the CEF binary distribution (no binaries committed).
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DownloadCEF)
DownloadCEF("${CEF_PLATFORM}" "stable" "${CEF_VERSION}"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef")

# Add the CEF CMake helpers to the module path and load them.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

# Build the libcef C++ wrapper library (static, part of the distribution).
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

#
# opennyx target.
#

set(OPENNYX_SRCS
  src/browser_window.cc
  src/browser_window.h
  src/opennyx_app.cc
  src/opennyx_app.h
  src/opennyx_client.cc
  src/opennyx_client.h
  src/store.cc
  src/store.h
  src/blocklist.cc
  src/blocklist.h
  src/pages.cc
  src/pages.h
  src/scheme_handler.cc
  src/scheme_handler.h
)

if(OS_WINDOWS)
  list(APPEND OPENNYX_SRCS src/main_win.cc win/opennyx.rc)
endif()

set(CEF_TARGET "opennyx")

# Logical target for the libcef import library.
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

# Determine the target output directory.
SET_CEF_TARGET_OUT_DIR()

if(OS_WINDOWS)
  # Executable target (WIN32 = GUI subsystem, no console window).
  add_executable(${CEF_TARGET} WIN32 ${OPENNYX_SRCS})
  SET_EXECUTABLE_TARGET_PROPERTIES(${CEF_TARGET})
  add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
  target_link_libraries(${CEF_TARGET} libcef_lib libcef_dll_wrapper
                        ${CEF_STANDARD_LIBS})

  target_include_directories(${CEF_TARGET} PRIVATE
                             "${CMAKE_CURRENT_SOURCE_DIR}/src"
                             "${CMAKE_CURRENT_SOURCE_DIR}")

  # Source files contain UTF-8 string literals (button glyphs).
  target_compile_options(${CEF_TARGET} PRIVATE "/utf-8")
  # The json header is large; MSVC needs /bigobj for the resulting objects.
  target_compile_options(${CEF_TARGET} PRIVATE "/bigobj")
  # CEF builds with exceptions + RTTI disabled (_HAS_EXCEPTIONS=0 /GR-). Tell
  # nlohmann/json to use assert/abort instead of throw so it compiles cleanly
  # in that mode.
  target_compile_definitions(${CEF_TARGET} PRIVATE JSON_NOEXCEPTION)

  # Embed the application manifests (common controls + supported OS list).
  ADD_WINDOWS_MANIFEST("${CMAKE_CURRENT_SOURCE_DIR}/win" "${CEF_TARGET}" "exe")

  # Copy CEF binaries (libcef.dll etc.) and resources (.pak files, locales)
  # next to the executable so the output directory is directly runnable.
  COPY_FILES("${CEF_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}"
             "${CEF_TARGET_OUT_DIR}")
  COPY_FILES("${CEF_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}"
             "${CEF_TARGET_OUT_DIR}")
endif()

# Display configuration settings.
PRINT_CEF_CONFIG()
