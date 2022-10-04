# avatar_interaction
The repository to hold all interaction related code for avatar

A simple program to capture from webcam like device and render it in SDL.

Inspired from https://github.com/marcosps/v4l2_webcam

## Dependencies
`sudo apt install libsdl2-dev`


`sudo apt install libsdl2-image-dev`

## Build (video streaming)

1. `cd video_audio_proc && mkdir build && cd build`  
2. `cmake .. && make`

The commands will generate an executable `v4l_sdl_stream` in the `build` folder.

## Usage
[Sender] Run `v4l_sdl_stream -d [device-name]` or just `v4l_sdl_stream` to capture from the default device.


[Receiver] Run `./ndi_recv`




