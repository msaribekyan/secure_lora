import os
import sys
import threading
import time
from prompt_toolkit import PromptSession
from prompt_toolkit.patch_stdout import patch_stdout
import serial
from serial.tools import list_ports

if sys.platform == "win32":
    import pyreadline3
else:
    import readline

serial_port = None
receive_thread = None
receive_running = False

session = PromptSession()

def receive_data():
    global serial_port, receive_running

    while receive_running and serial_port and serial_port.is_open:
        try:
            if serial_port.in_waiting:
                data = serial_port.readline()
                if data:
                    try:
                        message = data.decode("utf-8").strip()
                    except:
                        break
                    print(f"< {message}")

        except:
            print("\nSerial connection lost")
            receive_running = False
            serial_port = None
            break

        time.sleep(0.01)

def print_available_ports():
    all_ports = list(list_ports.comports())
    ports = [p for p in all_ports if p.vid is not None and p.pid is not None]

    if not ports:
        print("No serial devices found.")
        return

    for i, port in enumerate(ports):
        if port.device.startswith("/dev/ttyACM") or port.device.startswith("/dev/ttyUSB"):
            print(
                f"[{i}] {port.device:8} "
                f"{port.description} "
                f"(VID={port.vid}, PID={port.pid})"
            )

def connect(port):
    global serial_port, receive_thread, receive_running

    if serial_port and serial_port.is_open:
        print("Already connected")
        return

    try:
        serial_port = serial.Serial(port=port, baudrate=115200, timeout=0.1)

        receive_running = True

        receive_thread = threading.Thread(target=receive_data, daemon=True)
        receive_thread.start()

        print("Connected")

    except:
        print("Connection error")

def disconnect():
    global serial_port, receive_running, receive_thread

    if not (serial_port and serial_port.is_open):
        print("Not connected")
        return

    receive_running = False

    if receive_thread:
        receive_thread.join(timeout=1)
        receive_thread = None

    serial_port.close()
    serial_port = None

    print("Disconnected")

def clear():
    os.system("cls" if os.name == "nt" else "clear")

def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF

    for byte in data:
        crc ^= byte << 8

        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF

    return crc

def send_message(message: bytes):
    if serial_port is None:
        print("Not connected")
        return
    if len(message) > 127:
        print("Message too long, 127 bytes max")
        return
    type_byte = 0x80 | len(message)
    crc_data = bytes([type_byte]) + message
    crc = crc16_ccitt_false(crc_data)
    crc_bytes = crc.to_bytes(2, byteorder="little")

    packet = bytes([0x24]) + crc_data + crc_bytes
    serial_port.write(packet)
    serial_port.flush()

def print_help():
    print("--- Help ---")
    print("Connect to serial port before sending messages")
    print("To send a message, simply type in terminal")
    print("Available commands:")
    print("/devices - lists serial devices")
    print("/connect [port] - connect to specified serial port")
    print("/disconnect - disconnect from serial port")
    print("/clear - clear terminal")
    print("/exit - exit cli")

def main():
    print("Secure LoRa CLI")
    print("Type /help for available commands")

    while True:
        try:
            with patch_stdout():
                text_input = session.prompt("> ").strip()

            if not text_input:
                continue
            elif text_input == "/help":
                print_help()
            elif text_input == "/devices":
                print_available_ports()
            elif text_input.startswith("/connect"):
                parts = text_input.split()
                if len(parts) != 2:
                    print("Invalid /connect usage")
                    continue
                connect(parts[1])
            elif text_input == "/disconnect":
                disconnect()
            elif text_input == "/clear":
                clear()
            elif text_input == "/exit":
                break
            else:
                send_message(text_input.encode("utf-8"))
        except KeyboardInterrupt:
            break
    if serial_port and serial_port.is_open:
        disconnect()
    print("Exiting...")
    
if __name__ == "__main__":
    main()
