MAXIMUM_NODES = 32
MAX_HOP_DISTANCE = 10
SIZE_OF_CS_DATA =18
SIZE_OF_DATA =8
SIZE_OF_HEADER= 10
SIZE_OF_SN =4
START_INDEX_DATA= 10
MAX_RESERVE_SEQUENCE =10
DEVEICE_ID_LEN =8
DEVEICE_ID_SKIP_LEN =2
DISTANCE_VECTOR_SN =0
MPN_SN =1
PING_SN =3
advertisedInterval = 10000;
pingTimeout= 5000
resetRoutingInterval =30000
#$<00fb0a0a0000mo000006c>
myID = 0
MAC = "a098dghs"
MOIST_ID = 'mo'
radioString=""
advertiseNext = 0

lastAdvertised = 0
pingInitiatedTime = 0

import datetime

class DistanceVector:
    def __init__(self, destination):
        self.distance = MAX_HOP_DISTANCE
        self.next_hop = MAXIMUM_NODES
        self.destination = destination

class MPN:
    def __init__(self, id):
        self.available = True
        self.mac="FFFFFFFF"
        self.last_active=0
        self.id = id
        self.list_of_services = list()

class RFPACKET:
    def __init__(self):
        self.serialNo=0
        self.next_hop = MAXIMUM_NODES
        self.destination = MAXIMUM_NODES
        self.source = MAXIMUM_NODES
        self.data = list()

class MPNManager:
    def __init__(self):
        self.mpn = [MPN(mpn) for mpn in range(MAXIMUM_NODES)]
        self.mpn[myID].available = False
        self.mpn[myID].mac = MAC
        self.routing_table = [DistanceVector(node) for node in range(MAXIMUM_NODES)]
        self.routing_table[myID].destination = myID
        self.routing_table[myID].distance = 0
        self.routing_table[myID].next_hop = myID
        self.latest_packet = RFPACKET()

    def request_mpn(self, mac):
        print("MAC: {}".format(mac))
        for mpn in self.mpn:
            print("MAC: {}, {}".format(mac, mpn.mac))
            if mpn.mac == mac:
                mpn.mac = mac
                mpn.available = False
                mpn.last_active = datetime.datetime.now()
                return mpn.id
        for mpn in self.mpn:
            if mpn.available:
                mpn.mac = mac
                mpn.available = False
                mpn.last_active = datetime.datetime.now()
                return mpn.id
        return MAXIMUM_NODES

    def update_last_active(self, id):
        self.mpn[id].last_active = datetime.datetime.now()

    def ping_to_node(self, id):
        packet = "<"
        packet += '{:04x}'.format(PING_SN)
        packet += '{:02x}'.format(self.routing_table[id].next_hop)
        packet += '{:02x}'.format(id)
        packet += '{:02x}'.format(myID)
        packet += DEVICE_ID[:SIZE_OF_DATA]
        calculated_XRCS = 0
        byte_arr = bytes(packet, 'ascii')
        for i in range(SIZE_OF_CS_DATA):
            calculated_XRCS ^= byte_arr[i + 1]

        packet += '{:02x}'.format(calculated_XRCS)
        packet += ">"
        print(packet)
        return packet

    def forword_to_next_node_in_path(self):
        packet = "<";
        packet += '{:04x}'.format(self.latest_packet.serialNo)
        packet += '{:02x}'.format(self.routing_table[self.latest_packet.destination].next_hop)
        packet += '{:02x}'.format(self.latest_packet.destination)
        packet += '{:02x}'.format(self.latest_packet.source)

        for i in range(SIZE_OF_DATA):
            packet += self.latest_packet.data[i]
        calculated_XRCS = 0
        byte_arr = bytes(packet, 'ascii')
        for i in range(SIZE_OF_CS_DATA):
            calculated_XRCS ^= byte_arr[i + 1]
        packet += '{:02x}'.format(calculated_XRCS)
        packet += ">"
        print(packet)
        return packet


    def advertise_distance_vetor(self, destination):
        packet = "<"
        packet += '{:04x}'.format(DISTANCE_VECTOR_SN)
        packet += '{:02x}'.format(MAXIMUM_NODES)
        packet += '{:02x}'.format(MAXIMUM_NODES)
        packet += '{:02x}'.format(myID)
        packet += '{:02x}'.format(destination)
        packet += '{:02x}'.format(self.routing_table[destination].distance)
        packet += '{:02x}'.format(destination + 1)
        packet += '{:02x}'.format(self.routing_table[destination + 1].distance)
        calculated_XRCS = 0
        byte_arr = bytes(packet, 'ascii')
        for i in range(SIZE_OF_CS_DATA):
            calculated_XRCS ^= byte_arr[i + 1]
        packet += '{:02x}'.format(calculated_XRCS)
        packet += ">"
        print(packet)
        return packet


    def request_service_list_node(self, id):
        pass


    def update_routing_table(self, destination, distance, nexthop):
        print("destination: ", end=" ")
        print(destination)
        print("distance: ", end=" ")
        print(distance)
        print("nexthop: ", end=" ")
        print(nexthop, end=" ")
        self.mpn[nexthop].last_active =  datetime.datetime.now()
        if distance + 1 < self.routing_table[destination].distance :
            self.routing_table[destination].distance = distance + 1
            self.routing_table[destination].next_hop = nexthop
        elif self.routing_table[destination].next_hop == nexthop:
            self.routing_table[destination].distance = distance + 1
        self.print_routing_table()

    def print_routing_table(self):
        print("Routing Table")
        for i in range(MAXIMUM_NODES):
            print("|", end=" ")
            print(self.routing_table[i].destination, end=" ")
            print("|", end=" ")
            print(self.routing_table[i].distance, end=" ")
            print("|", end=" ")
            print(self.routing_table[i].next_hop, end=" ")
            print("|", end=" ")
            print(self.mpn[i].last_active, end=" ")
            print("|")
            print("-----------------")

    def packet_decode(self, rf_string):
        print("Decoding: {}".format(rf_string))
        cs_str = rf_string[19:21]
        received_cs = int(cs_str, 16)
        print("CS: {}".format(cs_str))
        calculated_XRCS = 0
        byte_arr = bytes(rf_string, 'ascii')
        for i in range(SIZE_OF_CS_DATA):
            calculated_XRCS ^= byte_arr[i + 1]
        if calculated_XRCS != received_cs:
            print("CS Invalid: Received({}) Calculated({})".format(received_cs, calculated_XRCS))
            return
        received_packet = RFPACKET()
        received_packet.serialNo = int(rf_string[1:5], 16)
        print("serial: ({})".format(rf_string[1:5]))
        print("next:({})({})".format(rf_string[5:7], rf_string[9:11]))
        received_packet.next_hop = int(rf_string[5:7], 16)
        received_packet.destination = int(rf_string[7:9], 16)
        received_packet.source = int(rf_string[9:11], 16)
        received_packet.data = rf_string[11:19]
        return received_packet

    def response_handler(self, rf_string):
        request = self.packet_decode(rf_string)
        print("Request NO {}".format(request.serialNo))
        if not request:
            return
        if request.serialNo == DISTANCE_VECTOR_SN:
            self.update_routing_table(int(str(request.data[0:2]), 16), int(request.data[2:4], 16), int(request.source))
            self.update_routing_table(int(str(request.data[4:6]), 16), int(request.data[6:8], 16), int(request.source))
            return
        if request.serialNo == MPN_SN:
            print("MPN")
            response = self.mpn_response(request)
            print("response:")
            return self.response_string(response)

        if request.serialNo == PING_SN:
            print("Ping ")
            return self.ping_response(request)

        elif not request.serialNo > self.latest_packet.serialNo:
            print("ignore")
            return
        else:
            print("Executing ...")

    def ping_response(self, request):
        response = RFPACKET()
        response.serialNo = request.serialNo + 1
        response.next_hop = self.routing_table[request.source].next_hop
        response.destination = request.source
        response.destination = myID
        response.data = DEVICE_ID[:8]
        return response

    def mpn_response(self, request):
        response = RFPACKET()
        mnp = '{:02x}'.format(self.request_mpn(request.data))
        print("MPN: {}".format(mnp))
        response.serialNo = MPN_SN + 1
        if(request.source < MAXIMUM_NODES and self.routing_table[request.source].next_hop < MAXIMUM_NODES):
            response.next_hop = self.routing_table[request.source].next_hop
        else:
            response.next_hop = request.source
        response.destination = request.source
        response.source = myID
        response.data = mnp+request.data[2:]
        return response

    def response_string(self, response):
        packet = "<";
        packet += '{:04x}'.format(response.serialNo)
        packet += '{:02x}'.format(response.next_hop)
        packet += '{:02x}'.format(response.destination)
        packet += '{:02x}'.format(response.source)

        for i in range(SIZE_OF_DATA):
            packet += response.data[i]
        calculated_XRCS = 0
        byte_arr = bytes(packet, 'ascii')
        for i in range(SIZE_OF_CS_DATA):
            calculated_XRCS ^= byte_arr[i + 1]
        packet += '{:02x}'.format(calculated_XRCS)
        packet += ">"
        print(packet)
        return packet
