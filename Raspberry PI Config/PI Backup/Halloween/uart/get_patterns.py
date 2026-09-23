import serial
import struct
import time
import json

results = {}

def writeData(data_one, data_two):

	for i in range(0, 32):
		if(i == 0):
			ser.write([
				0xA0, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00
			])
		elif(i == 31):
			ser.write([
				0x01, 0x00, data_one, data_two,
				0x00, 0x00, 0x00, 0x00
			])
		else:
			ser.write([
				0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00
			])

ser = serial.Serial(
    port='/dev/serial0',
    baudrate=57600,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

print(ser.isOpen())
time.sleep(1)

with open("patterns.json", "w") as outfile: 
	for i in range(0, 2):
		for j in range(0, 256):
			writeData(i*255, j)
			results[input("Pattern name: ")] = ['0x' + hex(i*255)[2:].zfill(2), '0x' + hex(j)[2:].zfill(2)]
			outfile.seek(0)
			json.dump(results, outfile)
			outfile.truncate()

exit()

ser.close()
