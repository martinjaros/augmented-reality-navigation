#!/usr/bin/python

# Parses HEX encoded IMU buffers from serial line

from struct import unpack
from binascii import unhexlify

tty = open("/dev/ttyACM3", "rb")
tty.readline()

while True:
    s = tty.readline().rstrip()
    if len(s) != 52:
        continue

    buf = unhexlify(s)
    acc1, acc2, acc3 = unpack('>hhh', buf[0:6])
    gyro1, gyro2, gyro3 = unpack('>hhh', buf[6:12])
    mag1, mag2, mag3 = unpack('>hhh', buf[12:18])
    ts = unpack('>Q', buf[18:26])[0]
    print "ACC [{:06d},{:06d},{:06d}], GYRO [{:06d},{:06d},{:06d}], MAG [{:06d},{:06d},{:06d}], TS = {:.3f}".format(acc1, acc2, acc3, gyro1, gyro2, gyro3, mag1, mag2, mag3, ts / 1e9)
