# avatar_interaction
The repository to hold all interaction related code for avatar

A simple program to capture from webcam like device and render it in SDL.

Inspired from https://github.com/marcosps/v4l2_webcam

## Build (video streaming)
**Before building, change the `dev_name` in `video_audio_proc/src/v4l_sdl.c` to the actual path of the device you want to capture.**  


1. `cd video_audio_proc && mkdir build && cd build`  
2. `cmake .. && make`

The commands will generate an executable `v4l_sdl_stream` in the `build` folder.

Run it view the video streaming from the camera.




