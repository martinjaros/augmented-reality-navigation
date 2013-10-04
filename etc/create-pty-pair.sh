#!/bin/sh

n=0
while [ -h pty${n}src ]
do
  n=$(($n+1))
done

echo "Creating PTY pair: pty${n}sink -> pty${n}src"
socat -d -d PTY,link=pty${n}src,raw,echo=0,wait-slave PTY,link=pty${n}sink,raw,echo=0
