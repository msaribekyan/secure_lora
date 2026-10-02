# mock the port for test
import serial

# fake port loop:// open
port = serial.serial_for_url("loop://", timeout=1)

port.write(b"hello\n")
reply = port.readline()
print(reply)
port.close()
