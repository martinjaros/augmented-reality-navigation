#!/bin/sh

TTY=pty0sink
if [ ! -h $TTY ]
then
  echo "$TTY does not exist"
  exit
fi

# Constants
PI=`echo "4*a(1)" | bc -l`
EARTH_RADIUS=6371000
NM2KM=1.852
KMH2MS=3.6

# Initial state (degrees, degrees, meters)
LATITUDE=0
LONGITUDE=0
ALTITUDE=100

# Gradient (s, mph, degrees, m/s)
DTIME=2
SPEED=10
TRACK=0
VARIO=1

while :
do
    TRK=`echo "$TRACK/180*$PI" | bc -l`
    LATITUDE=`echo "$LATITUDE + c($TRK)*$SPEED*$NM2KM*$KMH2MS*$DTIME/$EARTH_RADIUS*180/$PI" | bc -l`
    LONGITUDE=`echo "$LONGITUDE + s($TRK)*$SPEED*$NM2KM*$KMH2MS*$DTIME/$EARTH_RADIUS/c($LATITUDE/180*$PI)*180/$PI" | bc -l`
    ALTITUDE=`echo "$ALTITUDE + $VARIO*$DTIME" | bc`

    LAT_DEG=`echo "$LATITUDE / 1" | bc`
    LAT_MIN=`echo "$LATITUDE % 1 * 60" | bc`
    LON_DEG=`echo "$LONGITUDE / 1" | bc`
    LON_MIN=`echo "$LONGITUDE % 1 * 60" | bc`
    printf "\$GPGGA,%s,%02u%07.4f,N,%03u%02f,E,1,08,0.0,%.1f,M,0.0,M,,\r\n" $(date +"%H%M%S") $LAT_DEG $LAT_MIN $LON_DEG $LON_MIN $ALTITUDE | tee $TTY
    printf "\$GPRMC,%s,A,%02u%07.4f,N,%03u%02f,E,%.2f,%.2f,010170,0.0,E\r\n" $(date +"%H%M%S") $LAT_DEG $LAT_MIN $LON_DEG $LON_MIN $SPEED $TRACK | tee $TTY
    printf "\$GPRMB,A,0.00,L,TEST00,TEST01,0000.0000,N,00000.0000,E,0.0,0.0,0.0,V\r\n" | tee $TTY
    sleep $DTIME
done
