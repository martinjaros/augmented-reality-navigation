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
        rate = 0 #radians(.1)

        fifo = open(FIFO, "wb")
        starttime = time()
        try:
            while True:

                curtime = time() - starttime
                #data = [0, 0, 4096, 0, 0, rate / radians(1000) * 32767, 32767 * cos(curtime * rate), 32767 * sin(curtime * rate), 0]
                data = [0.34202014332567*4096,0.46984631039295*4096,0.81379768134937*4096,0,0,0,-0.16317591116653*32767,0.88256411925939*32767,-0.44096961052988*32767]
                fifo.write(pack('>' + 'h' * 9, *data))
                fifo.write(pack('Q', curtime * 1e9))
                fifo.flush()
                print "{:021.9f}".format(curtime)
                sleep(.01)

        except IOError:
            fifo.close()

except KeyboardInterrupt:
    unlink(FIFO)
