#!/usr/bin/python

# Emulates NMEA 0183 device by using FIFO

from os import mkfifo, unlink
from time import time, sleep, gmtime, strftime
from math import sqrt, sin, cos, atan2, modf, degrees, radians, pi

FIFO = "gpsfifo"
mkfifo(FIFO)

try:
    while True:

        # Initial position (rad, rad, m)
        lat = 49.229089 / 180.0 * pi
        lon = 16.556432 / 180.0 * pi
        alt = 270

        # Motion (rad, m/s, m/s)
        track = radians(0)
        speed = 100
        vario = 1

        # Waypoint (str, rad, rad)
        wpname = "TEST01"
        wplat = 49.256322 / 180.0 * pi
        #wplon = 16.557412 / 180.0 * pi
        wplon = 16.664412 / 180.0 * pi

        prevtime = time()
        def tick():
            global prevtime, lat, lon, alt

            sleep(.5)
            curtime = time()
            period = curtime - prevtime
            prevtime = curtime

            lat += cos(track) * speed * period / 6371000
            lon += sin(track) / cos(lat) * speed * period / 6371000
            alt += vario * period


        def checksum(msg):
            cs = 0
            for char in msg[1:]:
                cs ^= ord(char);
            return msg + '*' + "{:02X}".format(cs)

        fifo = open(FIFO, "wb")

        try:
            while True:

                # GGA
                tick()
                latf = modf(degrees(abs(lat)))
                lonf = modf(degrees(abs(lon)))
                msg = "$GPGGA," + strftime("%H%M%S", gmtime(prevtime))
                msg += ",{:02.0f}{:07.4f},{:s},{:03.0f}{:07.4f},{:s}".format(latf[1], latf[0] * 60, lat > 0 and "N" or "S", lonf[1], lonf[0] * 60, lon > 0 and "E" or "W")
                msg += ",1,08,0.0,{:.1f},M,0.0,M,,".format(alt)
                msg = checksum(msg)
                fifo.write(msg + "\r\n")
                fifo.flush()
                print msg

                # RMC
                tick()
                latf = modf(degrees(abs(lat)))
                lonf = modf(degrees(abs(lon)))
                msg = "$GPRMC," + strftime("%H%M%S", gmtime(prevtime))
                msg += ",A,{:02.0f}{:07.4f},{:s},{:03.0f}{:07.4f},{:s}".format(latf[1], latf[0] * 60, lat > 0 and "N" or "S", lonf[1], lonf[0] * 60, lon > 0 and "E" or "W")
                msg += ",{:.2f},{:.2f},".format(speed * 3.6/1.852, degrees(track)) + strftime("%d%m%y", gmtime(prevtime)) + ",0.0,E"
                msg = checksum(msg)
                fifo.write(msg + "\r\n")
                fifo.flush()
                print msg

                # RMB
                tick()
                dlat = wplat - lat
                dlon = cos(lat) * (wplon - lon)
                dist = sqrt(dlat * dlat + dlon * dlon) * 6371 / 1.852
                brg = degrees(atan2(dlon, dlat))
                wplatf = modf(degrees(abs(wplat)))
                wplonf = modf(degrees(abs(wplon)))
                msg = "$GPRMB,A,0.0,L,," + wpname
                msg += ",{:02.0f}{:07.4f},{:s},{:03.0f}{:07.4f},{:s}".format(wplatf[1], wplatf[0] * 60, wplat > 0 and "N" or "S", wplonf[1], wplonf[0] * 60, wplon > 0 and "E" or "W")
                msg += ",{:04.1f},{:04.1f},0.0,V".format(dist < 999 and dist or 999, brg < 0 and abs(360 - brg) or abs(brg))
                msg = checksum(msg)
                fifo.write(msg + "\r\n")
                fifo.flush()
                print msg

                # WPL
                tick()
                msg = "$GPWPL,{:02.0f}{:07.4f},{:s},{:03.0f}{:07.4f},{:s},".format(wplatf[1], wplatf[0] * 60, wplat > 0 and "N" or "S", wplonf[1], wplonf[0] * 60, wplon > 0 and "E" or "W")
                msg += wpname
                msg = checksum(msg)
                fifo.write(msg + "\r\n")
                fifo.flush()
                print msg

        except IOError:
            fifo.close()

except KeyboardInterrupt:
    unlink(FIFO)
