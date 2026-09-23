import serial
import struct
import time

ser = serial.Serial(
    port='/dev/serial0',
    baudrate=57600,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

print(ser.isOpen())
time.sleep(1)

# Draw large triangle pattern on projector 3
data = [
  0xA0, 0x10, 0x00, 0x00,
  0x0A, 0x00, 0x00, 0x00,
  0xA0, 0x00, 0x00, 0x00,
  0x00, 0xFF, 0x00, 0x00
]

ser.write(data)

ser.close()
