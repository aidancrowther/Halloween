import RPi.GPIO as GPIO
import os

shutdownPIN = 21

GPIO.setmode(GPIO.BCM)
GPIO.setup(shutdownPIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

shutdownStarted = 0

while True:
    if not(shutdownStarted) and not(GPIO.input(shutdownPIN)):
        shutdownStarted = 1
        os.system("sudo shutdown -h now")
