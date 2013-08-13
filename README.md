Augmented reality navigation
============================

This application uses data from GPS and inertial sensors to render a navigational overlay to captured video.

LICENSING
---------
Use with GNU/GPL v3 license

DEPENDENCIES
------------
Kernel
 * videodev2
 * i2c-dev

Libraries
 * freetype2
 * EGL
 * GLESv2
 * X11 (optional)

HARDWARE
--------
Peripherals:
 * Video camera (V4L2)
  - Either USB Video Class device or any other supported by kernel

 * Inertial sensors (I2C)
  - InvenSense ITG-3200 gyroscope
  - AKM AK8975 compass
  - Bosh BMA-150 accelerometer

 * GPS sensor (TTY)
  - Any NMEA 0183 compatible device with serial interface

COMPILATION
-------------
To change compiler (gcc by default) use
~~~
make clean
make CC=gcc-arm-linux-gnueabi
~~~
To enable X11 support use
~~~
make clean
make X11BUILD=1
~~~
To enable debug symbols use
~~~
make clean
make DEBUG=1
~~~
To change trace level use
~~~
make clean
make TRACE_LEVEL=number
~~~
Levels are
 * 0 - disabled
 * 1 - error
 * 2 - warning (default)
 * 3 - info
 * 4 - debug

To generate documentation use
~~~
make doc
~~~
