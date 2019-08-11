#define MAXIMUM_NODES 32
#define MAX_HOP_DISTANCE 10
#define SIZE_OF_CS_DATA 18
#define SIZE_OF_DATA 8
#define SIZE_OF_HEADER 10
#define SIZE_OF_SN 4
#define START_INDEX_DATA 10
#define MAX_RESERVE_SEQUENCE 10
#define DEVEICE_ID_LEN 8
#define DEVEICE_ID_SKIP_LEN 2
#define DISTANCE_VECTOR_SN 0
#define MPN_SN 1
#define PING_SN 3
int advertisedInterval = 10000;
#define pingTimeout 5000
#define resetRoutingInterval 30000
// inpout io
#define POWER_PIN 4
#define MOISTURE_PIN A0
#define REQUEST_TIMEOUT 3000
#include <EEPROM.h>

//$<00fb0a0a0000mo000006c>
struct dist_vector {
  byte distance = 126 ;
  byte next_hop = 127;
};

struct RF_PACKET  {
  unsigned int serial_no = 0;
  byte next_hop;
  byte destination;
  byte source;
  char data[SIZE_OF_DATA];
};


byte MY_MPN = MAXIMUM_NODES;
char DEVICE_ID[DEVEICE_ID_LEN + 1] = "AFFFFF02";
char ROOT_DEVICE_ID[DEVEICE_ID_LEN + 1] = "FFFFFFFF";
char MOIST_ID[2] = {'m', 'o'};
struct RF_PACKET latest_packet, request_pkt;
struct dist_vector routing_table[MAXIMUM_NODES];
String radioString;
byte advertiseNext = 0;

long lastAdvertised ;
long lastPingedFromRoot;
int pingInitiatedTime;
bool connected_to_server = false;
bool mpn_wait = true;
bool packet_valid = false;

void update_routing_table(byte destination, byte distance, byte nexthop)
{
  Serial.print("destination: ");
  Serial.println(destination);
  Serial.print("distance: ");
  Serial.println(distance);
  Serial.print("nexthop: ");
  Serial.println(nexthop);
  if (distance + 1 < routing_table[destination].distance) {
    routing_table[destination].distance = distance + 1 ;
    routing_table[destination].next_hop = nexthop;
    //is_route_changed = true;
  }
}

int getNum(char ch)
{
  int num = 0;
  if (ch >= '0' && ch <= '9')
  {
    num = ch - 0x30;
  }
  else
  {
    switch (ch)
    {
      case 'A': case 'a': num = 10; break;
      case 'B': case 'b': num = 11; break;
      case 'C': case 'c': num = 12; break;
      case 'D': case 'd': num = 13; break;
      case 'E': case 'e': num = 14; break;
      case 'F': case 'f': num = 15; break;
      default: num = 0;
    }
  }
  return num;
}

bool packet_decode() {
  byte calculated_XRCS = 0;
  for (int i = 0; i < SIZE_OF_CS_DATA; i++)
    calculated_XRCS ^= radioString[i];
  Serial.println(radioString); Serial.print("calculated_XRCS: "); Serial.println(calculated_XRCS);
  unsigned int received_XRCS = getNum(radioString[SIZE_OF_HEADER + SIZE_OF_DATA]) * 16 + getNum(radioString[SIZE_OF_HEADER + SIZE_OF_DATA + 1]);
  if (received_XRCS != calculated_XRCS)
    return false;
  request_pkt.serial_no = getNum(radioString[0]) * 4096 + getNum(radioString[1]) * 256 + getNum(radioString[2]) * 16 + getNum(radioString[3]);
  request_pkt.next_hop = getNum(radioString[4]) * 16 + getNum(radioString[5]);
  request_pkt.destination = getNum(radioString[6]) * 16 + getNum(radioString[7]);
  request_pkt.source = getNum(radioString[8]) * 16 + getNum(radioString[9]);
  for (byte i = 0; i <  SIZE_OF_DATA; i++)
    request_pkt.data[i] = radioString[START_INDEX_DATA + i];
  return true;
}

bool request_arrived()
{
  char c;
  while (Serial.available()) {
    Serial.print("Searching pkt ..");
    c = Serial.read();
    Serial.println(c);
    if (c == '<') {
      radioString = Serial.readStringUntil('>');
      Serial.println("Message Packet found ..");
      return packet_decode();
    }
  }
  return false;
}

void request_mpn_from(byte node_id) {
  String packet = "<";
  packet +=  "0001";
  packet += node_id < 16 ? "0" + String(node_id, HEX) : String(node_id, HEX); 
  packet += MAXIMUM_NODES < 16 ? "0" + String(MAXIMUM_NODES, HEX) : String(MAXIMUM_NODES, HEX);
  packet += MAXIMUM_NODES < 16 ? "0" + String(MAXIMUM_NODES, HEX) : String(MAXIMUM_NODES, HEX);
  for (byte i = 0; i < SIZE_OF_DATA; i++)
    packet +=  DEVICE_ID[i];
  byte calculated_XRCS = 0;
  for (int i = 1; i < SIZE_OF_CS_DATA + 1; i++)
    calculated_XRCS ^= packet[i];
  packet +=  calculated_XRCS < 16 ? "0" + String(calculated_XRCS, HEX) : String(calculated_XRCS, HEX);
  packet += ">";
  Serial.println(packet);
  //Serial3.println(packet);
  return;
}

bool is_valid_mpn() {
  for (byte i = 2; i < SIZE_OF_DATA; i++) {
    if (request_pkt.data[i] != DEVICE_ID[i]) {
      return false;
    }
  }
  return true;
}

bool mpn_response() {
  long start_time = millis();
  while (millis() - start_time < REQUEST_TIMEOUT ) {
    if (request_arrived()) {
      if (request_pkt.serial_no == DISTANCE_VECTOR_SN)
        update_routing_table();
      if (request_pkt.serial_no == MPN_SN + 1 && is_valid_mpn()) {
        unsigned int mpn = getNum(request_pkt.data[0]) * 16 + getNum(request_pkt.data[1]);
        if (mpn < MAXIMUM_NODES) {
          MY_MPN = mpn;
          return true;
        }
      }
    }
  }
  return false;
}

void mpn_connect() {
  byte node_id = 0;
  Serial.println("Searching ....");
  request_mpn_from(node_id);
  while (!mpn_response()) {
    digitalWrite(13, !digitalRead(13));
    if (node_id == MAXIMUM_NODES - 1)
      node_id = 0;
    Serial.print("\trequest send to ");
    Serial.println(node_id);
    request_mpn_from(node_id);
    node_id += 1;
  }
}
void printRoutingTable() {
  Serial.println("Routing Table");
  for (int i = 0; i < MAXIMUM_NODES; i++) {
    Serial.print("|");
    Serial.print(i);
    Serial.print("|");
    Serial.print(routing_table[i].distance);
    Serial.print("|");
    Serial.print(routing_table[i].next_hop);
    Serial.print("|");
    Serial.println("\n-----------");
  }
}
void update_routing_table() {
  unsigned int destination = getNum(request_pkt.data[0]) * 16 + getNum(request_pkt.data[1]);
  unsigned int distance =  getNum(request_pkt.data[2]) * 16 + getNum(request_pkt.data[3]);
  unsigned int nexthop = request_pkt.source;
  if (destination != MY_MPN)
    update_routing_table(destination, distance, nexthop);

  destination = getNum(request_pkt.data[4]) * 16 + getNum(request_pkt.data[5]);
  distance =  getNum(request_pkt.data[6]) * 16 + getNum(request_pkt.data[7]);
  if (destination != MY_MPN)
    update_routing_table(destination, distance, nexthop);

  printRoutingTable();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  //Serial3.begin(9600);
  pinMode(POWER_PIN, INPUT);
  pinMode(MOISTURE_PIN, OUTPUT);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  routing_table[MY_MPN].distance = 0;
  routing_table[MY_MPN].next_hop = MY_MPN;
  //if root is set , myId is set, if root is availble
  Serial.println("Initialised ..");
  mpn_connect();
  Serial.println("Connected ..");
}

void loop() {

}
