/*

*/
//#include <SPI.h>
//const byte READ =     0b11111111;   //0xFF
//const byte CMD_POLL = 0b00111001;   //0x39
//const byte END_READ = 0b00001111;   //0x0F

//setup pins
const byte CLOCK = 2;
const byte MOSI = 3;
const byte MISO = 4;

byte MOSI_buffer;
byte MISO_buffer;

byte bitsRead, 
unsigned long millisLastClock;

bool readingMOSIMessage;  //set when new MOSI message detected
bool readingMISOMessage;  //set when new MISO message detected
bool pollingSlave;
byte bytesToRead;
byte checksum;

byte[20] message;
byte messageLength;
string messageSource;
byte MOSI_last_byte;
byte MISO_acknowledge;


//LIFO queue for strings to send over serial
string[5] stringQueue;
const byte QUEUE_SIZE = 5;
byte stringQueuePushIndex;
byte stringQueuePopIndex;

void setup() {
    //SPI.begin();
    bitsRead = 0;
    bytesToRead = 0xFF;
    readingMOSIMessage = false;
    readingMISOMessage = false;
    pollingSlave = false;
    stringQueuePushIndex = 0;
    stringQueuePopIndex = 0;
    pinMode(MOSI, INPUT);
    pinMode(MISO, INPUT);
    pinMode(CLOCK, INPUT);
    attachInterrupt(digitalPinToInterrupt(CLOCK), clockPulse, FALLING);
    
    serial.begin(9600);
}

void loop() {
  
    //clear bit counter between each byte of traffic on SPI bus (only necessary if gets out of sync)
    if(millis() - millisLastClock > 2){
      bitsRead = 0;
      MOSI_buffer = 0x00;
      MISO_buffer = 0x00;
    }
    
    //check if a string is queued
    if(stringQueuePopIndex != stringQueuePushIndex){
      sendMessage(stringQueue[stringQueuePopIndex]);
      stringQueuePopIndex++;
      if(stringQueuePopIndex == QUEUE_SIZE) stringQueuePopIndex = 0;
    }
}

void clockPulse(){
  MOSI_buffer = (MOSI_buffer << 1) + digitalRead(MOSI);
  MISO_buffer = (MISO_buffer << 1) + digitalRead(MISO);
  bitsRead++;
  if(bitsRead == 8){
    bitsRead = 0;
    
    //not in the middle of any message, this is the first byte recorded on the bus
    if( !readingMOSIMessage && !readingMISOMessage && MOSI_buffer == 0xAA){  //look for start of outgoing message
      readingMOSIMessage = true;
      return;
    }
    if( !readingMOSIMessage && !readingMISOMessage && MOSI_buffer == 0xC6){  //indicates master is polling slave
      pollingSlave = true;
      return;
    }
    
    //we read the first byte captured on the bus and it indicated master was polling slave
    if(pollingSlave){
      if MISO_buffer != 0xFF && MISO_buffer != 0x00){       //0xFF is not ready, 0x00 is no data to send
        bytesToRead = MISO_buffer + 1;                      //message + checksum
        messageLength = MISO_buffer;
        readingMISOMessage = true;
        pollingSlave = false;
        return;
      }
    }
    
    //readingMISOMessage means we already have captured the number of bytes to read from the slave
    if(readingMISOMessage){
      bytesToRead--;
      if(bytesToRead > 0){
        message[messageLength-bytesToRead] = MISO_buffer
      }
      else{
        checksum = MISO_buffer;
        readingMISOMessage = false;
        bytesToRead = 0xFF;           //indicates it is not set yet
        messageSource = "Motor Controller: ";
        processMessage();
        return;
      }
    }
    
    //we have just started or are in the process of intercepting a message from the master
    if(readingMOSIMessage){
      //bytes haven't been read yet, we are at beginning of message
      if(bytesToRead == 0xFF){  
        bytesToRead = MOSI_buffer + 2  (add 2 bytes for checksum and response)
        return; 
      }
      //we are in middle of reading data of message
      else if(bytesToRead > 2){
        message[messageLength-(bytesToRead-2)] = MOSI_buffer;
        bytesToRead--;
        return;
      }
      //we are at end of message (checksum)
      else if(bytesToRead == 2){
        checksum = MOSI_buffer;
        bytesToRead--;
        return;
      //we have read checksum, now checking for acknowledgement
      else if(bytesToRead == 1){
        MOSI_last_byte = MOSI_buffer;
        MISO_acknowledge = MISO_buffer;
        bytesToRead = 0xFF;
        readingMOSIMessage = false;
        messageSource = "Logic board: ";
        processMessage();
      }
    }    
  }
}

void processMessage(){
  string msg;
  if(isValidMessage()){
    msg = messageSource;
    for(byte i=0;i<messageLength;i++){
      msg += String(message[i], HEX) + " ";
    }
    queueMessage(msg);
  }
}

bool isValidMessage(){
  byte calcChecksum = 0x00;
  for(byte i=0; i< messageLength; i++){
    calcChecksum = calcChecksum ^ message[i];
  }
  if(calcChecksum == checksum)
    return true;
  else
    return false;
}

void queueMessage(string msg){
  stringQueue[stringQueuePushIndex] = msg;
  stringQueuePushIndex++;
  if(stringQueuePushIndex == QUEUE_SIZE ) stringQueuePushIndex = 0;
}

void sendMessage(string msg){
  serial.println(msg);
}