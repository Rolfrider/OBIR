#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

RF24 radio(7,8);                // nRF24L01(+) radio CE and CSN to 7th and 8th port 
RF24Network network(radio);      // network uses that radio

const int OUR_CHANNEL = 65;
const uint16_t THIS_NODE_ID = 00;    // address of our node in Octal format ( 04,031, etc)
const uint16_t OTHER_NODE_ID = 01;   // address of the other node in Octal format

//payload resources
const int ALL = 0;
const int LAMP = 1;
const int KEYBOARD = 2;
//payload values
const char GETrf = 'g';
const char OFF = '0';
const char ON = '1';

struct payload_t {                 // structure of our payload
  unsigned long ms;
  unsigned int resource; //0-lamp 1-keyboard
  char value; //g-get 0-off 1-on
};

char lastKeyPressed = '0';
bool isLampOn = false;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  radio.begin();
  network.begin(OUR_CHANNEL, THIS_NODE_ID);
  Serial.println("############# OBIR PROJECT: ARDUINO UNO #############");
}

void loop() {
   network.update();

   while ( network.available() ) {     // is there anything ready for us?
      RF24NetworkHeader header;        // if so, grab it and handle
      payload_t payload;
      network.read(header,&payload,sizeof(payload));
      Serial.print("Received packet at ");
      Serial.println(payload.ms);
      handlePayload(payload);
    }   
}

void getLampState(){
  Serial.println("Sending lamp state request.");
  payload_t payload {millis(), LAMP, GET};
  send(payload);
}

void setLampState(char state){
  Serial.print("Sending lamp state change to ");
  if(state == ON){
    Serial.println("on.");
  }else{
    Serial.println("off.");
  }
  payload_t payload {millis(), LAMP, state};
  send(payload);
}

void getKeyboardState(){
  Serial.println("Sending keyboard state request.");
  payload_t payload {millis(), KEYBOARD, GET};
  send(payload);
}

void handlePayload(payload_t payload){
Serial.print("MESSAGE: ");
  switch (payload.resource) {
       case LAMP:
         Serial.print("Lamp is ");
         switch (payload.value) {
           case ON:
             isLampOn = true;
             Serial.println("on.");
             break;
           case OFF:
             isLampOn = false;
             Serial.println("off.");
             break;
           default:
             Serial.println("(Unknown lamp state!).");
             break;
         }
         break;
       case KEYBOARD:
          lastKeyPressed = payload.value;
          Serial.print("Keyboard's last key pressed is \"");
          Serial.print(payload.value);
          Serial.println("\"");
          break;
       default:
         Serial.println("Unknown message");
         break;
  }
}

bool send(payload_t payload){
    Serial.print(F("Sending..."));
    RF24NetworkHeader header(OTHER_NODE_ID);
    bool ok = network.write(header,&payload,sizeof(payload));
    if (ok){
     Serial.println("ok.");
     return true;
    }
    else{
     Serial.println("failed.");
     return false;
    }
}
