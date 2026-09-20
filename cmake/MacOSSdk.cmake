# Run before project(): vcpkg probes its compiler while loading the toolchain.
# Pass the same SDK to those subprocesses and to this project's compiler.
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  return()
endif()

if(NOT "${CMAKE_OSX_SYSROOT}" STREQUAL "")
  set(devtools_macos_sdk "${CMAKE_OSX_SYSROOT}")
elseif(NOT "$ENV{SDKROOT}" STREQUAL "")
  set(devtools_macos_sdk "$ENV{SDKROOT}")
else()
  set(devtools_macos_sdk macosx)
endif()

# Resolve SDK names through the active developer directory (xcode-select or
# DEVELOPER_DIR), rather than guessing a Command Line Tools installation path.
if(NOT IS_ABSOLUTE "${devtools_macos_sdk}")
  execute_process(
    COMMAND /usr/bin/xcrun --sdk "${devtools_macos_sdk}" --show-sdk-path
    RESULT_VARIABLE devtools_sdk_result
    OUTPUT_VARIABLE devtools_macos_sdk
    ERROR_VARIABLE devtools_sdk_error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT devtools_sdk_result EQUAL 0)
    message(FATAL_ERROR "Cannot locate the macOS SDK using xcrun: ${devtools_sdk_error}")
  endif()
endif()
if(NOT IS_DIRECTORY "${devtools_macos_sdk}")
  message(FATAL_ERROR "macOS SDK does not exist: ${devtools_macos_sdk}. Select an installed SDK with -DCMAKE_OSX_SYSROOT=...")
endif()

set(CMAKE_OSX_SYSROOT "${devtools_macos_sdk}" CACHE PATH "macOS SDK used by DevTools and vcpkg" FORCE)
set(ENV{SDKROOT} "${devtools_macos_sdk}")
message(STATUS "macOS SDK: ${devtools_macos_sdk}")
