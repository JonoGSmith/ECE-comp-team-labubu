import sys
import glob

import serial # pip install pyserial

gActiveDevice: serial.Serial | None

# TODO: No idea what happens when it disconnects

def try_attach():
    global gActiveDevice
    gActiveDevice = find_pico()
    if gActiveDevice is None:
        return None

    print(f"[com  ] Attached to {gActiveDevice.name}")

def rotate_servo(angle: float):
    angle = max(min(angle, 90), -90)
    send_msg("servo {angle}")

def send_msg(s: str):
    global gActiveDevice
    if gActiveDevice is not None:
        gActiveDevice.write((s+'\n').encode())

# Background task (TODO: run in a thread)
def recv_loop():
    global gActiveDevice
    if gActiveDevice is None:
        return
    while True:
        s = gActiveDevice.read_until().decode()
        if "Button 0: pressed" in s:
            pass # TODO: GOGOGO start the converting
        if "Button 0: released" in s:
            pass # TODO: GOGOGO stop the converting

def find_pico():
    for p in serial_ports():
        print(f"Testing device {p.name}")
        p.write(b"areyouthepico?\n")
        p.write_timeout = 1
        while True:
            response = p.read_until()
            if len(response) == 0:
                break
            l = response.decode()
            if "yes" in l:
                return p
    return None

def serial_ports():
    """ Lists serial port names
        https://stackoverflow.com/questions/12090503/listing-available-com-ports-with-python

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result: list[serial.Serial] = []
    for port in ports:
        try:
            s = serial.Serial(port)
            result.append(s)
        except (OSError, serial.SerialException):
            pass
    return result

#Button - GP3 (pressed = low)
#Servo -GP002 pwm 