#!/usr/bin/python

# GPS bridged over UDP socket

from os import mkfifo, unlink
from socket import socket, AF_INET, SOCK_DGRAM, SOL_SOCKET, SO_REUSEADDR

FIFO = "gpsfifo"
mkfifo(FIFO)

try:
    while True:

        fifo = open(FIFO, "wb")
        sock = socket(AF_INET, SOCK_DGRAM)
	sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
	sock.bind(("localhost", 4353))
        try:
            while True:
                s = sock.recv(256)
                fifo.write(s)
                fifo.flush()
                print s

        except IOError:
            fifo.close()

except KeyboardInterrupt:
    unlink(FIFO)
