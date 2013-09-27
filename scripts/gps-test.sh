#!/bin/sh
TTY=/dev/pts/6

LAT=0
LON=0
ALT=100

LAT_INC=0.00005
LON_INC=0
ALT_INC=0

SPD=100
TRK=10

RNG=5.2
BRG=20

while :
do
    LAT=$(echo "$LAT + $LAT_INC" | bc)
    LAT_DEG=$(echo "$LAT / 1" | bc)
    LAT_MIN=$(echo "$LAT % 1 * 60" | bc)
    LON=$(echo "$LON + $LON_INC" | bc)
    LON_DEG=$(echo "$LON / 1" | bc)
    LON_MIN=$(echo "$LON % 1 * 60" | bc)
    ALT=$(echo "$ALT + $ALT_INC" | bc)
    NANOSEC=$(echo $(date +%N) / 1000000 | bc)
    printf "\$GPGGA,%s.%03u,%02u%07.4f,N,%03u%02f,E,1,08,0.0,%.1f,M,0.0,M,,\r\n" $(date +"%H%M%S") $NANOSEC $LAT_DEG $LAT_MIN $LON_DEG $LON_MIN $ALT | tee $TTY
    printf "\$GPRMC,%s.%03u,A,%02u%07.4f,N,%03u%02f,E,%.2f,%.2f,010170,0.0,E\r\n" $(date +"%H%M%S") $NANOSEC $LAT_DEG $LAT_MIN $LON_DEG $LON_MIN $SPD $TRK | tee $TTY
    printf "\$GPRMB,A,0.00,L,TEST00,TEST01,0000.0000,N,00000.0000,E,%.2f,%05.1f,0.0,V\r\n" $RNG $BRG | tee $TTY
    sleep .1
done
