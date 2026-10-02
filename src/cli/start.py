# send and receive in parallel using threads

import threading
import serial

# fake port loop://
port = serial.serial_for_url("loop://", timeout=0.2)
running = threading.Event()
running.set()

def reader():
    while running.is_set():
        line = port.readline()
        if line:
            print("\nreceived:", line.decode().strip())
            print("> ", end="", flush=True)

# launch the reader thread 
# daemon so it doesnt block the main thread
thread = threading.Thread(target=reader, daemon=True)
thread.start()

try:
    while True:
        text = input("> ")
        port.write(text.encode() + b"\n")
finally:
    running.clear()
    thread.join()
    port.close()