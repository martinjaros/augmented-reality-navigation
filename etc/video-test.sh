#!/bin/sh

# Emulates video device by using V4L2 loopback

sudo modprobe v4l2loopback

gst-launch-1.0 videotestsrc pattern=solid-color foreground-color=0xE0F0E0 ! \
"video/x-raw,format=RGBx,width=800,height=600,framerate=20/1,interlace-mode=progressive" ! \
v4l2sink device=/dev/video0
