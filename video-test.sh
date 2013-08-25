#!/bin/sh
gst-launch-1.0 videotestsrc ! "video/x-raw,format=(string)RGBx,width=(int)800,height=(int)600,framerate=(fraction)20/1,interlace-mode=(string)progressive,pixel-aspect-ratio=(fraction)1/1" ! v4l2sink device=/dev/video0
