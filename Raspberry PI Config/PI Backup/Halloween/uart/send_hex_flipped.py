import serial
import struct
import time

def delay_5us():
    for aaa in range (0,235):
        pass

ser = serial.Serial(
    port='/dev/serial0',
    baudrate=57600,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

print(ser.isOpen())
#time.sleep(1)

# Draw large triangle pattern on projector 3
data = [
  0x30, 0x34, 0x40, 0x00,
  0x21, 0xE0, 0x00, 0x01
]

data_one = [
  0xA0, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
]

data_two = [
  0x20, 0x34, 0x10, 0x01,
  0x21, 0xE0, 0x00, 0x01
]

ser.write(data)
#delay_5us()
#time.sleep(1)
#ser.write(data_one)
#delay_5us()
ser.write(data_two)

ser.close()
