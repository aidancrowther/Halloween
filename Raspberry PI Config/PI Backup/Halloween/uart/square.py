import time
import serial

ser = serial.Serial('/dev/serial0', 57600, timeout=0.050)

ser.write(b'\xF0\x4C\x00\x01')

ser.write(b'\x32\x19\x38\x01')
ser.write(b'\x96\x19\x38\x00')
ser.write(b'\x96\x4B\x38\x01')
ser.write(b'\x32\x4B\x38\x00')

for i in range(46):
    ser.write(b'\x00\x00\x00\x00')
