cmake_minimum_required(VERSION 3.14)

project(RobotVisionConsole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Windows version definition
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# Set FFmpeg path
set(FFMPEG_ROOT "../../ffmpeg-7.1-full_build-shared")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")

# Set asio path
set(ASIO_ROOT "${CMAKE_SOURCE_DIR}/../../asio-1.30.2/include")
set(ASIO_INCLUDE_DIR "${ASIO_ROOT}")

add_executable(RobotVisionConsole
  main.cpp
  ../src/CameraCapture.cpp
  ../src/CameraDataReceiver.cpp
  ../src/CameraDataSender.cpp
  ../src/H264Decoder.cpp
  ../src/H264Encoder.cpp
  ../src/FFmpegUtils.cpp
  ../src/H264NALUParser.cpp
  ../src/NetworkVideoSource.cpp
)

# Use modern way to set include directories and library directories
target_include_directories(RobotVisionConsole PRIVATE
    ${FFMPEG_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/../src
)

target_link_directories(RobotVisionConsole PRIVATE ${FFMPEG_LIB_DIR})

# Link FFmpeg libraries
target_link_libraries(RobotVisionConsole
    avcodec
    avformat
    avdevice
    swscale
    avutil
)

include(GNUInstallDirs)
install(TARGETS RobotVisionConsole
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
