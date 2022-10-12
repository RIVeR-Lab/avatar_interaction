cmake_minimum_required(VERSION 3.12.0)
project(video_audio_proc C CXX)

add_compile_options(-std=c++11 -o-O20 -Wall)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(Threads REQUIRED)
find_package(libconfig++ REQUIRED)

#Find ffmpeg directories
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
pkg_check_modules(_FFMPEG_AVFORMAT libavformat)
pkg_check_modules(_FFMPEG_AVCODEC libavcodec)
pkg_check_modules(_FFMPEG_AVUTIL libavutil)
pkg_check_modules(_FFMPEG_SWSCALE libswscale)
# pkg_check_modules(_PC_X264 x264)
endif (PKG_CONFIG_FOUND)

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
  ${LIBCONFIG++_LIBRARIES}
  ${CMAKE_SOURCE_DIR}/3rdparty/lib/x86_64-linux-gnu/libndi.so
  avformat
  avcodec
  avutil
  swscale
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

add_executable(ndi_send
  src/ndi_send.cpp
)

target_link_libraries(ndi_send
  ${SDL2_LIBRARIES}
  ${SDL2_IMAGE_LIBRARIES}
  ${CMAKE_SOURCE_DIR}/3rdparty/lib/x86_64-linux-gnu/libndi.so
  ${LIBCONFIG++_LIBRARIES}
  avformat
  avcodec
  avutil
  swscale
  libasound.so
)

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
