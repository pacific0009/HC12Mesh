import re
import serial
import threading
from mpn import MPNManager
import time

class Controller:
    def __init__(self):
        self.mpn_manager = MPNManager()
        self.mpn_manager.print_routing_table()
        # load data form db

    def run(self):
        t = threading.Thread(target=self.handle_serial_input)
        t.setDaemon(True)
        t.start()
        while True:
            time.sleep(2)
            print('.')

    def handle_serial_input(self):
        ser = serial.Serial('/dev/ttyS0', baudrate=9600, timeout=None)
        while True:
            try:
                p = re.compile('<.*>')
                packet = str(ser.readline())
                result = p.search(packet)
                if result:
                    print("Packet data: {}".format(result.group(0)))
                    response = self.mpn_manager.response_handler(result.group(0))
                    print("response: {}".format(response))
                    ser.write(response.encode())
                else:
                    print(packet)
            except Exception as e:
                print(e)

controller = Controller()
controller.run()

