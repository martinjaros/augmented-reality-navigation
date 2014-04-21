#!/usr/bin/python

# Gyroscope calibration

from struct import unpack
from binascii import unhexlify

tty = open("/dev/ttyACM3", "rb")
tty.readline()

count, sum1, sum2, sum3 = 0.0, 0, 0, 0

while True:
    s = tty.readline().rstrip()
    if len(s) != 52:
        continue

    gyro1, gyro2, gyro3 = unpack('>hhh', unhexlify(s)[6:12])
    sum1 += gyro1
    sum2 += gyro2
    sum3 += gyro3
    count += 1

    print "{:06.2f} {:06.2f} {:06.2f}".format(sum1 / count, sum2 / count, sum3 / count)
