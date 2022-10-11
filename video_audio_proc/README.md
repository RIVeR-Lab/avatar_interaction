## Dependencies
`source setup_env.sh`


### Install libconfig
`cd 3rdparty/`  
`tar -xf libconfig-1.7.3.tar.gz`  
`cd libconfig-1.7.3`  
`./configure`  
`make & sudo make install`  

## Build (video streaming)

1. `cd video_audio_proc && mkdir build && cd build`  
2. `cmake .. && make`

The commands will generate an executable `v4l_sdl_stream` in the `build` folder.

## Usage
[Sender] Run `v4l_sdl_stream -d [device-name]` or just `v4l_sdl_stream` to capture from the default device.


[Receiver] Run `./ndi_recv`



Sender: ffmpeg -f v4l2 -framerate 60 -video_size 1920x1080 -input_format mjpeg -i /dev/video4 -preset faster -pix_fmt yuv420p -f mpegts -flush_packets 0 udp://127.0.0.1:23000
Sender: ffmpeg -f v4l2 -framerate 60 -video_size 1920x1080 -input_format mjpeg -i /dev/video3 -preset ultrafast -f mpegts -flush_packets 0 -threads 6 -tune zerolatency udp://127.0.0.1:23000 


Receiver: ffplay  udp://127.0.0.1:23000 -fflags nobuffer

  Helpful links:
  https://trac.ffmpeg.org/wiki/StreamingGuide#Latency
  https://stackoverflow.com/questions/47292785/recording-from-webcam-using-ffmpeg-at-high-framerate



