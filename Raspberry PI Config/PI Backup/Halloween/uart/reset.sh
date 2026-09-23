#!/bin/bash

#insmod /home/pi/soft_uart/soft_uart.ko gpio_tx=10 gpio_rx=3
#stty -F /dev/ttySOFT0 9600

raspi-gpio set 11 op
raspi-gpio set 11 dl
sleep 1
raspi-gpio set 11 dh
