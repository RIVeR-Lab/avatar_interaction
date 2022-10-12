# avatar_interaction
The repository to hold all interaction related code for avatar

A simple program to capture from webcam like device and render it in SDL.


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
    - [ ] ~~Try capturing device with MMAP mode~~
- [ ] NDI video processing
    - [x] MJPEG format support (SO HARD!!!!) Switch to FFMPEG might be the option
    - [ ] Send audio along with video
    - [x] How to generate 60fps video?? Maybe play with the buffer too? (solved with better router)
    - [ ] ~~YUYV format support~~
    - [ ] Crop 
    - [ ] Resolution
    - [x] Solve memory leak in MJPEG mode
- [ ] AV integration
    - [x] Integrate 3 mics, 3 cameras, 1 speaker on robot side
    - [x] Integrate 1 camera, 1 headphone on operator side
    - [x] assess audio video desync
    - [ ] AEC implementation

- [ ] ffmpeg for video streaming
    - [ ] Tunning ffmpeg for different streaming options: resolution, ratio, codec, fps, stream protocol

- [ ] Try Ultragrid
    - [ ] Use Ultragrid to transmit Jabra and Insta with compression.

Sender: ffmpeg -f v4l2 -framerate 60 -video_size 1920x1080 -input_format mjpeg -i /dev/video4 -preset faster -pix_fmt yuv420p -f mpegts -flush_packets 0 udp://127.0.0.1:23000

Sender(GPU): ffmpeg -f v4l2 -framerate 60 -video_size 1920x1080 -input_format mjpeg -hwaccel cuda -i /dev/video2 -f mpegts -flush_packets 0 udp://127.0.0.1:23000

Receiver: ffplay  udp://127.0.0.1:23000 -fflags nobuffer
ffplay udp://127.0.0.1:23000 -fflags nobuffer -framedrop -flags low_delay


  Helpful links:
  https://trac.ffmpeg.org/wiki/StreamingGuide#Latency
  https://stackoverflow.com/questions/47292785/recording-from-webcam-using-ffmpeg-at-high-framerate


Inspired from https://github.com/marcosps/v4l2_webcam

