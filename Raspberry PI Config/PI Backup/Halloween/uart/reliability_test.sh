#!/bin/bash

#./init_serial.sh

while true; do

    python3 square.py
    sleep 15
    python3 stop.py
    sleep 15

done
