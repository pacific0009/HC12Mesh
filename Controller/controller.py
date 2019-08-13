import re
import serial
import threading
from mpn import MPNManager
import time
import datetime

class Controller:
    def __init__(self):
        self.mpn_manager = MPNManager()
        self.mpn_manager.print_routing_table()
        self._last_advertised = 0
        self.ser = serial.Serial('/dev/ttyS0', baudrate=9600, timeout=None)
        # load data form db

    def run(self):
        t = threading.Thread(target=self.handle_serial_input)
        t.setDaemon(True)
        t.start()
        while True:
            time.sleep(2)
            print('.')

    def advertise_mpn_routing_table(self):
        print("ADEVERTISING Routing Table")
        _last_advertised = datetime.datetime.now()
        start_time = datetime.datetime.now()
        diff = datetime.datetime.now() - start_time
        print("Diff {}".format(diff.seconds))
        while (diff.seconds < 2):
            response = self.mpn_manager.response_next_distance_vector()
            print("Response DV {}".format(response))
            self.ser.write(response.encode())
            time.sleep(0.2)
            diff = datetime.datetime.now() - start_time

    def handle_serial_input(self):
        while True:
            try:
                p = re.compile('<.*>')
                packet = str(self.ser.readline())
                result = p.search(packet)
                if result:
                    print("Packet data: {}".format(result.group(0)))
                    response = self.mpn_manager.response_handler(result.group(0))
                    print("response: {}".format(response))
                    self.ser.write(response.encode())
                else:
                    print(packet)
            except Exception as e:
                print(e)
            diff = datetime.datetime.now() - self._last_advertised
            if(diff.seconds > 120):
                self.advertise_mpn_routing_table()


controller = Controller()
controller.run()
controller.advertise_mpn_routing_table()

