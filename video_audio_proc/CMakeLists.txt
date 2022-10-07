cmake_minimum_required(VERSION 3.1.0)
project(video_audio_proc C CXX)

add_compile_options(-std=c++11 -O2 -Wall)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_BUILD_TYPE Release)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(Threads REQUIRED)

include_directories(
  include
  3rdparty/include
  ${SDL2_INCLUDE_DIRS}
  ${SDL2_IMAGE_INCLUDE_DIRS}
  )

add_executable(v4l2_ndi_send
  src/v4l2_ndi_send.cpp
)

target_link_libraries(v4l2_ndi_send
  ${SDL2_LIBRARIES}
  ${SDL2_IMAGE_LIBRARIES}
  ${CMAKE_SOURCE_DIR}/3rdparty/lib/x86_64-linux-gnu/libndi.so
)

add_executable(alsa_ndi_send
  src/alsa_ndi_send.cpp
)

target_link_libraries(alsa_ndi_send
  libasound.so
  ${CMAKE_SOURCE_DIR}/3rdparty/lib/x86_64-linux-gnu/libndi.so
)


add_executable(ndi_recv
  src/ndi_recv.cpp
)

target_link_libraries(ndi_recv
  libasound.so
  ${SDL2_LIBRARIES}
  ${SDL2_IMAGE_LIBRARIES}
  ${CMAKE_SOURCE_DIR}/3rdparty/lib/x86_64-linux-gnu/libndi.so
)

# add_executable(ma
#   src/audio_proc.cpp
# )
# target_link_libraries(audio_proc
#   m
#   pthread
#   libdl.so
# )

add_executable(alsa_capture
  src/alsa_capture.c
)

target_link_libraries(alsa_capture
  libasound.so
)

file(GLOB audio_test RELATIVE ${CMAKE_SOURCE_DIR} "src/audio_*.c")
foreach(test_src ${audio_test})
  get_filename_component(target ${test_src} NAME_WE)
  message(${target})
  add_executable(${target}
    ${test_src}
  )
  target_link_libraries(${target}
    libasound.so 
  )
endforeach()
