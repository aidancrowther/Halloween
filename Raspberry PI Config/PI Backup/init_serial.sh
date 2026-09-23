#!/bin/bash

insmod /home/pi/RpiSoft-UART/softuart.ko
#stty -F /dev/ttySOFT0 4800

raspi-gpio set 11 op
raspi-gpio set 11 dl
sleep 1
raspi-gpio set 11 dh
sleep 1
raspi-gpio set 11 dl
sleep 1
raspi-gpio set 11 dh
