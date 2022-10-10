# avatar_interaction
The repository to hold all interaction related code for avatar

A simple program to capture from webcam like device and render it in SDL.

Inspired from https://github.com/marcosps/v4l2_webcam

## Dependencies
`sudo apt install libsdl2-dev`

`sudo apt install libsdl2-image-dev`

### Install libconfig
`cd 3rdparty/libconfig-1.7.3/`  
`./configure`  
`make & sudo make install`  

## Build (video streaming)

1. `cd video_audio_proc && mkdir build && cd build`  
2. `cmake .. && make`

The commands will generate an executable `v4l_sdl_stream` in the `build` folder.

## Usage
[Sender] Run `v4l_sdl_stream -d [device-name]` or just `v4l_sdl_stream` to capture from the default device.


[Receiver] Run `./ndi_recv`



## TODO
Avatar Audio Video Processing
- [x] Setup the new router
- [ ] NDI audio processing
    - [x] Understand how snd_pcm_readi() works
    - [x] Understand period, frame, buffer, sample rate, bit depth
    - [x] Create a local capture and playback program 
    - [x] Send out NDI packet
    - [x] Receive NDI packet
    - [x] Solve underrun issue (solved by adding a delay on purpose)
    - [ ] Try capturing device with MMAP mode
- [ ] NDI video processing
    - [x] MJPEG format support (SO HARD!!!!) Switch to FFMPEG might be the option
    - [ ] Send audio along with video
    - [ ] How to generate 60fps video?? Maybe play with the buffer too?
    - [ ] YUYV format support
    - [ ] Crop 
    - [ ] Resolution
- [ ] AV integration
    - [ ] Integrate 3 mics, 3 cameras, 1 speaker on robot side
    - [ ] Integrate 1 camera, 1 headphone on operator side
    - [ ] assess audio video desync
    - [ ] AEC implementation

- [ ] ffmpeg for video streaming
    - [ ] Tunning ffmpeg for different streaming options: resolution, ratio, codec, fps, stream protocol

Sender: ffmpeg -f v4l2 -framerate 60 -video_size 1920x1080 -input_format mjpeg -i /dev/video4 -preset faster -pix_fmt yuv420p -f mpegts -flush_packets 0 udp://127.0.0.1:23000

Receiver: ffplay  udp://127.0.0.1:23000 -fflags nobuffer

  Helpful links:
  https://trac.ffmpeg.org/wiki/StreamingGuide#Latency
  https://stackoverflow.com/questions/47292785/recording-from-webcam-using-ffmpeg-at-high-framerate



