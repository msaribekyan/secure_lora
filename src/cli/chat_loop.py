import serial

# fake port loop://
port = serial.serial_for_url("loop://", timeout=1)

while True:
    text = input("> ")

    # port only accepts bytes so encode the text
    # add a \n so readline works
    port.write(text.encode() + b"\n")

    reply = port.readline()
    print("received: ", reply.decode().strip())

port.close()