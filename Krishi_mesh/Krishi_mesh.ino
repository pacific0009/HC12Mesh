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


#include <EEPROM.h>

//$<00fb0a0a0000mo000006c>
struct dist_vector {
  byte distance = 126 ;
  byte next_hop = 127;
};

struct RF_PACKET  {
  unsigned int serialNo = 0;
  byte next_hop;
  byte destination;
  byte source;
  char data[SIZE_OF_DATA];
};


byte myID = MAXIMUM_NODES;
char DEVICE_ID[DEVEICE_ID_LEN + 1] = "FFFFFFFF";
char ROOT_DEVICE_ID[DEVEICE_ID_LEN + 1] = "FFFFFFFF";
char MOIST_ID[2] = {'m', 'o'};
struct RF_PACKET latest_packet;
struct dist_vector routing_table[MAXIMUM_NODES];
String radioString;
byte advertiseNext = 0;

long lastAdvertised ;
long lastPingedFromRoot;
int pingInitiatedTime;
bool connected_to_server = True;

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


void rxRadio()
{
  char c;
  while (Serial.available()) {
    Serial.print("Searching < Found: ");
    c = Serial.read();
    Serial.println(c);
    if (c == '<') {
      radioString = Serial.readStringUntil('>');
      Serial.println("Message Packet found ..");
      processMessage();
      //processRadioString(c);
    }

  }
}

void processMessage() {
  Serial.println(radioString);
  byte calculated_XRCS = 0;
  for (int i = 0; i < SIZE_OF_CS_DATA; i++)
    calculated_XRCS ^= radioString[i];
  Serial.print("calculated_XRCS: ");
  Serial.println(calculated_XRCS);
  unsigned int received_XRCS = getNum(radioString[SIZE_OF_HEADER + SIZE_OF_DATA]) * 16 + getNum(radioString[SIZE_OF_HEADER + SIZE_OF_DATA + 1]);
  Serial.print("received_XRCS: ");
  Serial.println(received_XRCS);
  if (received_XRCS != calculated_XRCS)
    return;

  unsigned int serialNo = getNum(radioString[0]) * 4096 + getNum(radioString[1]) * 256 + getNum(radioString[2]) * 16 + getNum(radioString[3]);
  Serial.print("Sequence No");
  Serial.println(serialNo);
  if ( serialNo == 1 || (serialNo > MAX_RESERVE_SEQUENCE && serialNo < latest_packet.serialNo) )
    return;
  latest_packet.serialNo = serialNo;
  latest_packet.next_hop = getNum(radioString[4]) * 16 + getNum(radioString[5]);
  latest_packet.destination = getNum(radioString[6]) * 16 + getNum(radioString[7]);
  latest_packet.source = getNum(radioString[8]) * 16 + getNum(radioString[9]);
  for (byte i = 0; i <  SIZE_OF_DATA; i++)
    latest_packet.data[i] = radioString[START_INDEX_DATA + i];
  printLastPacket() ;
  if (serialNo == 0) {
    unsigned int destination = getNum(radioString[10]) * 16 + getNum(radioString[11]);
    unsigned int distance = getNum(radioString[12]) * 16 + getNum(radioString[13]);
    unsigned int nexthop = latest_packet.source;

    update_routing_table(destination, distance, nexthop);
    printRoutingTable();
    return;
  }

  if (serialNo == 2) {
    for (byte i = 0; i < 4 ; i++) {
      Serial.print(radioString[SIZE_OF_SN + i] );
      Serial.println(DEVICE_ID[SIZE_OF_SN + i] );
      if (radioString[SIZE_OF_SN + i] != DEVICE_ID[SIZE_OF_SN + i]) {
        Serial.println("Defice id not matched");
        return;
      }
    }
    myID = latest_packet.source;
    for (byte i = 0; i <  SIZE_OF_DATA; i++)
      ROOT_DEVICE_ID[i] = latest_packet.data[i] ;
    updateCredential();
    return;
  }


  latest_packet.serialNo = serialNo;

  for (byte i = 0; i <  SIZE_OF_DATA; i++)
    latest_packet.data[i] = radioString[START_INDEX_DATA + i];

  if (latest_packet.destination == myID && serialNo % 2 == 0) {
    onReceivedAcknowledge() ;
    return;
  }

  if (latest_packet.destination == myID) {
    //command for me
    Serial.println("Executing ...");
    onReceivedRequest();
    return;
  }

  if (latest_packet.next_hop == myID) {

    Serial.println("Forwarding ...");
    forwordToNextNodeInPath();

  }
  Serial.println("None of all ...");
}

void onReceivedAcknowledge() {

}

void onReceivedRequest() {
  if (latest_packet.source == 0) {
    lastPingedFromRoot = millis();
  }
  if (latest_packet.serialNo == MPN_SN) {
    for (byte i = 0; i <  SIZE_OF_DATA; i++)
      latest_packet.data[i] = DEVICE_ID[i];
  }
  else if (latest_packet.serialNo == PING_SN) {
    for (byte i = 0; i <  SIZE_OF_DATA; i++)
      ROOT_DEVICE_ID[i] = (char)latest_packet.data[i];
    sendAcknowledge();
  }
  else if (latest_packet.data[2] == MOIST_ID[0] && latest_packet.data[3] == MOIST_ID[1]) {

    digitalWrite(POWER_PIN, LOW);
    analogRead(MOISTURE_PIN);
    String moistVal = String(analogRead(MOISTURE_PIN), HEX);
    digitalWrite(POWER_PIN, HIGH);
    int len = moistVal.length();
    moistVal = len == 4 ? moistVal : len == 3 ? "0" + moistVal : len == 2 ? "00" + moistVal : "000" + moistVal;
    latest_packet.serialNo = latest_packet.serialNo + 1;
    latest_packet.next_hop = routing_table[latest_packet.source].next_hop;
    latest_packet.destination = latest_packet.source;
    latest_packet.source   = myID ;
    latest_packet.data[4] = moistVal[0];
    latest_packet.data[5] = moistVal[1];
    latest_packet.data[6] = moistVal[2];
    latest_packet.data[7] = moistVal[3];
  }

  sendAcknowledge();
}

void sendAcknowledge() {
  printLastPacket();
  String responseNo = String(latest_packet.serialNo, HEX);
  int len = 4 - responseNo.length();
  String packet = "<";
  packet +=  len == 4 ? responseNo : len == 3 ? "0" + responseNo : len == 2 ? "00" + responseNo : "000" + responseNo;
  packet +=  latest_packet.next_hop < 16 ? "0" + String(routing_table[latest_packet.source].next_hop , HEX) : String(routing_table[latest_packet.source].next_hop, HEX);
  packet +=  latest_packet.destination < 16 ? "0" + String(latest_packet.destination, HEX) : String(latest_packet.destination, HEX);
  packet +=  latest_packet.source < 16 ? "0" + String(latest_packet.source, HEX) : String(latest_packet.source, HEX);
  for (byte i = 0; i <  SIZE_OF_DATA; i++)
    packet += (char) latest_packet.data[i];//process command here generate response
  byte calculated_XRCS = 0;
  for (int i = 1; i < SIZE_OF_CS_DATA + 1; i++)
    calculated_XRCS ^= packet[i];
  packet +=  calculated_XRCS < 16 ? "0" + String(calculated_XRCS, HEX) : String(calculated_XRCS, HEX);
  packet += ">";
  Serial.println(packet);
  return;
}


void forwordToNextNodeInPath() {
  String responseNo = String(latest_packet.serialNo);
  int len = 4 - responseNo.length();
  String packet = "<";
  packet +=  len == 4 ? responseNo : len == 3 ? "0" + responseNo : len == 2 ? "00" + responseNo : "000" + responseNo;
  packet +=  routing_table[latest_packet.destination].next_hop < 16 ? "0" + String(routing_table[latest_packet.destination].next_hop , HEX) : String(routing_table[latest_packet.destination].next_hop, HEX);
  packet +=  latest_packet.destination < 16 ? "0" + String(latest_packet.destination, HEX) : String(latest_packet.destination, HEX);
  packet +=  latest_packet.source < 16 ? "0" + String(latest_packet.source, HEX) : String(latest_packet.source, HEX);
  for (byte i = 0; i <  SIZE_OF_DATA; i++)
    packet += (char) latest_packet.data[i];//process command here generate response
  byte calculated_XRCS = 0;
  for (int i = 1; i < SIZE_OF_CS_DATA + 1; i++)
    calculated_XRCS ^= packet[i];
  packet +=  calculated_XRCS < 16 ? "0" + String(calculated_XRCS, HEX) : String(calculated_XRCS, HEX);
  packet += ">";
  Serial.println(packet);
  return;
}

void advertiseDistanceVetorOf(byte destination) {
  String packet = "<";
  packet += "0000";
  packet += "FF";
  packet += "FF";
  packet +=  myID < 16 ? "0" + String(myID, HEX) : String(myID, HEX);
  packet +=  destination < 16 ? "0" + String(destination, HEX) : String(destination, HEX);
  packet +=  routing_table[destination].distance < 16 ? "0" + String(routing_table[destination].distance, HEX) : String(routing_table[destination].distance, HEX);
  packet +=  destination+1 < 16 ? "0" + String(destination+1, HEX) : String(destination+1, HEX);
  packet +=  routing_table[destination + 1].distance < 16 ? "0" + String(routing_table[destination + 1].distance, HEX) : String(routing_table[destination + 1].distance, HEX);
  byte calculated_XRCS = 0;
  for (int i = 1; i < SIZE_OF_CS_DATA + 1; i++)
    calculated_XRCS ^= packet[i];
  packet +=  calculated_XRCS < 16 ? "0" + String(calculated_XRCS, HEX) : String(calculated_XRCS, HEX);
  packet += ">";
  Serial.println(packet);
  Serial.println(calculated_XRCS);
  return;
}

void requestHeartBeatFrom(byte nodeId) {
  String packet = "<";
  packet +=  "0001";
  if (routing_table[nodeId].distance > MAX_HOP_DISTANCE)
    packet +=  nodeId < 16 ? "0" + String(nodeId, HEX) : String(nodeId, HEX);
  else
    packet +=  routing_table[nodeId].next_hop < 16 ? "0" + String(routing_table[nodeId].next_hop , HEX) : String(routing_table[nodeId].next_hop, HEX);
  packet +=  nodeId < 16 ? "0" + String(nodeId, HEX) : String(nodeId, HEX);
  packet +=  myID < 16 ? "0" + String(myID, HEX) : String(myID, HEX);
  packet +=  "00000000";
  byte calculated_XRCS = 0;
  for (int i = 1; i < SIZE_OF_CS_DATA + 1; i++)
    calculated_XRCS ^= packet[i];
  packet +=  calculated_XRCS < 16 ? "0" + String(calculated_XRCS, HEX) : String(calculated_XRCS, HEX);
  packet += ">";
  Serial.println(packet);
  Serial.println(calculated_XRCS);
  return;
}

void reset_routing_table() {
  Serial.println("Resseting Routing Table");
  for (int i = 0; i < MAXIMUM_NODES; i++) {
    routing_table[i].distance = MAX_HOP_DISTANCE;
    routing_table[i].next_hop = MAXIMUM_NODES + 1;
  }
  routing_table[myID].distance = 0;
  routing_table[myID].next_hop = myID;
  printRoutingTable();
  lastPingedFromRoot = millis();
}
void unpair() {
  myID = MAXIMUM_NODES;
  for (byte i = 0; i < DEVEICE_ID_LEN; i++)
    ROOT_DEVICE_ID[i] = 'F';
  updateCredential();
  return;
}
void updateCredential() {
  //EEPROM.put( 0,  DEVICE_ID);
  int eeAddress = sizeof(DEVICE_ID);
  EEPROM.put( eeAddress,  ROOT_DEVICE_ID);
  eeAddress += sizeof(ROOT_DEVICE_ID);
  EEPROM.put( eeAddress,  myID);
  return;
}
void loadCredentials() {
  EEPROM.get( 0,  DEVICE_ID);
  int eeAddress = sizeof(DEVICE_ID);
  EEPROM.get( eeAddress,  ROOT_DEVICE_ID);
  eeAddress += sizeof(ROOT_DEVICE_ID);
  EEPROM.get( eeAddress,  myID);

}
void printCredentials() {
  Serial.print("DEVICE_ID:");
  Serial.println(DEVICE_ID);
  Serial.print("ROOT_DEVICE_ID:");
  Serial.println(ROOT_DEVICE_ID);
  Serial.print("myID:");
  Serial.println(myID);
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

void printLastPacket() {
  Serial.print("serial: ");
  Serial.println(latest_packet.serialNo);
  Serial.print("next_hop: ");
  Serial.println(latest_packet.next_hop);
  Serial.print("destination: ");
  Serial.println(latest_packet.destination);
  Serial.print("source: ");
  Serial.println(latest_packet.source);
  Serial.print("data: ");
  Serial.println(latest_packet.data);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.begin(9600);

  pinMode(POWER_PIN, INPUT);
  pinMode(MOISTURE_PIN, OUTPUT);

  printCredentials();
  loadCredentials();
  printCredentials();
  lastAdvertised = millis();
  pingInitiatedTime = millis();
  routing_table[myID].distance = 0;
  routing_table[myID].next_hop = myID;
  //if root is set , myId is set, if root is availble
  Serial.println("Initialised ..");
}

void loop() {
  // put your main code here, to run repeatedly:
  rxRadio();
  if (millis() - lastAdvertised > advertisedInterval) {
    lastAdvertised = millis();
    advertiseNext >  MAXIMUM_NODES ? advertiseNext = 0 : advertiseNext = advertiseNext + 2;
    advertiseDistanceVetorOf(advertiseNext);
  }
  if (millis() - lastPingedFromRoot > resetRoutingInterval) {
    reset_routing_table();
  }
  //check your nearest neibours alive
  //set unreachable nodes
}
