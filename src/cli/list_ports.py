# print serial ports to stdout
from serial.tools import list_ports

ports = list_ports.comports()

for p in ports:
    # port name, vendor id, product id
    print(p.device, p.vid, p.pid) 