import time
import serial

ser = serial.Serial('/dev/serial0', 57600, timeout=0.050)

ser.write(b'\xF0\x0C\x00\x00')

for i in range(50):
    ser.write(b'\x00\x00\x00\x00')
