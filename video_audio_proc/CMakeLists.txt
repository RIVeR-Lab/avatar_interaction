cmake_minimum_required(VERSION 3.1.0)
project(video_audio_proc)

add_compile_options(-std=c++11 -O2 -Wall)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

include_directories(
  3rdparty/include
  ${SDL2_INCLUDE_DIRS}
  ${SDL2_IMAGE_INCLUDE_DIRS}
  )

add_executable(v4l_sdl_stream
  src/v4l_sdl.cpp
)

target_link_libraries(v4l_sdl_stream
  ${SDL2_LIBRARIES}
  ${SDL2_IMAGE_LIBRARIES}
  ${CMAKE_SOURCE_DIR}/3rdparty/lib/x86_64-linux-gnu/libndi.so
)



