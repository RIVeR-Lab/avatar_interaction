cmake_minimum_required(VERSION 3.0.2)
project(avatar_operator)

add_compile_options(-std=c++11 -O0 -Wall)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_BUILD_TYPE RelWithDebInfo)

# SDL libraries
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2TTF REQUIRED)
find_package(libconfig++ REQUIRED)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  std_msgs
  franka_control
)

catkin_package(
 INCLUDE_DIRS include
#  LIBRARIES avatar_operator
 CATKIN_DEPENDS roscpp std_msgs geometry_msgs franka_control message_runtime
#  DEPENDS system_lib
)

include_directories(
  ${catkin_INCLUDE_DIRS}
  include
  3rdparty/ndi/include
  ${SDL2_INCLUDE_DIRS}
  ${SDL2_IMAGE_INCLUDE_DIRS}
  ${SDL2TTF_INCLUDE_DIR}
  )


add_executable(ndi_recv_ui_node
  src/ndi_recv_ui_node.cpp
)

add_dependencies(ndi_recv_ui_node
  ${catkin_EXPORTED_TARGETS}
)

target_link_libraries(ndi_recv_ui_node
  ${SDL2_LIBRARIES}
  ${SDL2_IMAGE_LIBRARIES}
  ${SDL2TTF_LIBRARY}
  ${LIBCONFIG++_LIBRARIES}
  ${CMAKE_SOURCE_DIR}/3rdparty/ndi/lib/x86_64-linux-gnu/libndi.so
  ${catkin_LIBRARIES}
  rt # shm_open
)
