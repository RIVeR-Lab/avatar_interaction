#! /usr/bin/env bash

affix="&& bash || bash"
wd="~/avatar_ws/src/avatar_interaction/video_audio_proc/build/"
len=600

# ping
terminator -b --geometry=${len}x${len}+0+0 -e "ping 127.0.0.1 ${affix}" --working-directory=${wd} &
# htop
terminator -b --geometry=${len}x${len}+0+$len -e "htop ${affix}" --working-directory=${wd} &
# black magic
terminator -b --geometry=${len}x${len}+$len+0 -e "pwd ${affix}" --working-directory=${wd} & 
# spatial
terminator -b --geometry=${len}x${len}+$len+$len -e "htop ${affix}" --working-directory=${wd} & 
# # operator audio
terminator -b --geometry=${len}x${len}+$((2*len))+0 -e "htop ${affix}" --working-directory=${wd} & 
# # operator video
terminator -b --geometry=${len}x${len}+$((2*len))+$len -e "htop ${affix}" --working-directory=${wd} &