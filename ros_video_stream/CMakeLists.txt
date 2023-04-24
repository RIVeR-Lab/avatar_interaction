cmake_minimum_required(VERSION 3.0.2)
project(ros_video_stream)

## Compile as C++11, supported in ROS Kinetic and newer
# add_compile_options(-std=c++11)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  roscpp
  image_transport
  cv_bridge
)

find_package(OpenCV REQUIRED)


catkin_package(
 INCLUDE_DIRS include
#  LIBRARIES ros_video_stream
#  CATKIN_DEPENDS roscpp
#  DEPENDS system_lib
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(
# include
  ${catkin_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)

add_executable(streamer
  nodes/streamer.cpp
)
target_link_libraries(streamer
  ${catkin_LIBRARIES}
  ${OpenCV_LIBS}
)