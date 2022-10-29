#! /usr/bin/env bash
sudo apt install -y libsdl2-dev libsdl2-image-dev libavcodec-dev libavformat-dev libswscale-dev libsdl2-ttf-dev

echo "export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib" >> ~/.bashrc

