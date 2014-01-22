#!/usr/bin/python

# Emulates IIO device (acc, gyro, mag) by using FIFO

from os import mkfifo, unlink
from time import time, sleep
from struct import pack
from math import sin, cos, radians

FIFO = "imufifo"
mkfifo(FIFO)

try:
    while True:

        # Turn rate (rad/s)
        rate = radians(.1)

        fifo = open(FIFO, "w")
        starttime = time()
        try:
            while True:

                curtime = time() - starttime
                data = [0, 0, 32767, 0, 0, rate / radians(1000) * 32767, 32767 * cos(curtime * rate), 32767 * sin(curtime * rate), 0]
                fifo.write(pack('>' + 'h' * 9, *data))
                fifo.write(pack('Q', curtime * 1e9))
                fifo.flush()
                print "{:021.9f}".format(curtime)
                sleep(.1)

        except IOError:
            fifo.close()

except KeyboardInterrupt:
    unlink(FIFO)
