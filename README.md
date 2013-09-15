Augmented reality navigation
============================

This application uses data from GPS and inertial sensors to render a navigational overlay to captured video.
Configuration can be loaded at runtime by
~~~
./arnav config.cfg
~~~

LICENSING
---------
Use with GNU/GPL v3 license

DEPENDENCIES
------------
Kernel
 * videodev2
 * iio

Libraries
 * freetype2
 * EGL
 * GLESv2
 * X11 (optional)

HARDWARE
--------
Peripherals:
 * Video camera (supported by V4L2)
 * Inertial sensors (supported by IIO)
 * GPS sensor (NMEA 0183 compatible)

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
