# send and receive in parallel using threads
import threading
import serial
import argparse

def reader(port, running):
    while running.is_set():
        line = port.readline()
        if line:
            # need to verify format
            print("\nreceived:", line.decode().strip())
            print("> ", end="", flush=True)

def main():

    parser = argparse.ArgumentParser(description="CLI for secure LoRa nodes over USB serial")
    parser.add_argument("--port", default="loop://", help="serial port name")
    args = parser.parse_args()

    # pass the --port arg and open it
    port = serial.serial_for_url(args.port, 115200, timeout=0.2)
    running = threading.Event()
    running.set()

    # daemon so it doesnt block the main thread
    thread = threading.Thread(target=reader, args=(port, running), daemon=True)
    thread.start()

    try:
        while True:
            text = input("> ")
            port.write(text.encode() + b"\n")
    finally:
        running.clear()
        thread.join()
        port.close()


if __name__ == "__main__":
    main()