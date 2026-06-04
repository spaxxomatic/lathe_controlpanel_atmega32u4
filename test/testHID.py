# pip install hid
import hid
d = hid.device()
d.open(0x03eb, 0x2ff4)  # your VID/PID from usb.h
d.set_nonblocking(1)
try:
    while True:
        data = d.read(64, timeout_ms=100)
        if data:
            print(data)
except KeyboardInterrupt:
    pass
finally:
    d.close()