Augmented reality navigation
============================

This application uses data from GPS and inertial sensors to render a navigational overlay to captured video.

DEPENDENCIES
------------
Kernel
 * videodev2
 * i2c-dev

Libraries
 * freetype2
 * EGL
 * GLESv2
 * X11 (optional, use -DX11BUILD compiler flag to enable X11 support)

HARDWARE
--------
Peripherals:
 * Video camera (V4L2)
  - Either USB Video Class device or any other supported by kernel

 * Inertial sensors (I2C)
  - InvenSense ITG-3200 gyroscope

 * GPS sensors (TTY)
  - Any NMEA 0183 compatible device with serial interface

Target platforms:
 * BeagleBone
  - AM3358 ARM Cortex-A8
  - <http://beagleboard.org/Products/BeagleBone>

 * BeagleBoard
  - OMAP3530 ARM Cortex-A8
  - <http://beagleboard.org/Products/BeagleBoard>

 * PandaBoard
  - OMAP4460 ARM Cortex-A9 
  - <http://pandaboard.org/>

 * Raspberry Pi
  - BCM2835 ARM1176JZF-S
  - <http://www.raspberrypi.org/>
