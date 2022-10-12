#! /usr/bin/env bash

affix="&& bash || bash"
wd="~/avatar_ws/src/avatar_interaction/video_audio_proc/build/"
len=600

# ping
terminator --geometry=${len}x${len}+0+0 -e "ping 192.168.0.1 ${affix}" --working-directory=${wd} -T ping &
# htop
terminator --geometry=${len}x${len}+0+$len -e "htop ${affix}" --working-directory=${wd} -T htop &
# black magic
terminator --geometry=${len}x${len}+$len+0 -e "./ndi_recv -i 'Black Magic' ${affix}" --working-directory=${wd} -T bm & 
# operator video
terminator --geometry=${len}x${len}+$len+$len -e "./v4l2_ndi_send -c ../configs/insta360.config ${affix}" --working-directory=${wd} -T op_vid & 
# operator audio
terminator --geometry=${len}x${len}+$((2*len))+0 -e "./alsa_ndi_send -i plughw:1,0 -o operator ${affix}" --working-directory=${wd} -T op_aud & 
# spatial
terminator --geometry=${len}x${len}+$((2*len))+$len -e "./ndi_recv -i spatial -a plughw:2,0 ${affix}" --working-directory=${wd} -T spatial &