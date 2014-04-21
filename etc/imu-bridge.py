#!/usr/bin/python

# FIFO bridged over serial line

from os import mkfifo, unlink
from binascii import unhexlify
from struct import pack, unpack

TTY = "/dev/ttyACM3"
FIFO = "imufifo"
mkfifo(FIFO)

try:
    while True:

        fifo = open(FIFO, "wb")
        tty = open(TTY, "rb")
        tty.readline()
        try:
            while True:
                s = tty.readline().rstrip()
                print s
                if len(s) != 52:
                    continue

                buf = unhexlify(s)
                fifo.write(buf[0:6] + pack('>h', min(32767, -unpack('>h', buf[6:8])[0])) + buf[8:18] + pack('<Q', unpack('>Q', buf[18:26])[0]))
                fifo.flush()

        except IOError:
            fifo.close()

except KeyboardInterrupt:
    unlink(FIFO)
