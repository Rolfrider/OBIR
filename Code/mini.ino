#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <Keypad.h>

RF24 radio(9,10);                // nRF24L01(+) radio CE and CSN to 7th and 8th port
RF24Network network(radio);      // network uses that radio

const int OUR_CHANNEL = 65;
const uint16_t THIS_NODE_ID = 01;    // address of our node in Octal format ( 04,031, etc)
const uint16_t OTHER_NODE_ID = 00;   // address of the other node in Octal format

const unsigned long interval = 10000; //ms  // How often to send
unsigned long last_sent;             // when did we last send?

//payload resources
const int LAMP = 0;
const int KEYBOARD = 1;
//payload values
const char GET = 'g';
const char OFF = '0';
const char ON = '1';

struct payload_t {                // structure of our payload
  unsigned long ms;
  unsigned int resource;          // 0-lamp 1-keyboard
  char value;                     // g-get 0-off 1-on
};

char keys[4][3] = {
 {'*','0','#'},
 {'7','8','9'},
 {'4','5','6'},
 {'1','2','3'}
};

byte rowPins[4] = {5, 7, 8, 3};    // connect to the row pinouts of the keypad
byte colPins[3] = {4, 2, 6};       // connect to the column pinouts of the keypad
Keypad keypad=Keypad(makeKeymap(keys), rowPins, colPins, 4, 3);

char lastKeyPressed = '0';
char lastKeySent = '0';
bool isLampOn = false;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  radio.begin();
  network.begin(OUR_CHANNEL, THIS_NODE_ID);
  pinMode(A0, OUTPUT);              // lamp on/off pin
  digitalWrite(A0, HIGH);           // setting lamp off at start
  Serial.println("############# OBIR PROJECT: ARDUINO MINI PRO #############");
}

void loop() {
   network.update();
   while (network.available()) {       // is there anything ready for us?
      RF24NetworkHeader header;        // if so, grab it and handle
      struct payload_t payload;
      network.read(header,&payload,sizeof(payload));
      Serial.print("Received packet at ");
      Serial.println(payload.ms);
      handlePayload(payload);
   }
   
   char key = keypad.getKey();      // checking if any key has been pressed
   if (key != NO_KEY){
      Serial.print("\"");
      Serial.print(key);
      Serial.println("\" key has been pressed.");
      lastKeyPressed = key;
      last_sent = 0;
   }
   
   if(lastKeyPressed != lastKeySent){   // if key has changed, send info
      unsigned long now = millis()+interval;
      if (now-last_sent>=interval){
        if(!sendKeyboardState()){     // if failed to send key, try again after set interval of time
          last_sent = now;
        }
      }
   }
}

bool handlePayload(payload_t payload){
  Serial.print("REQUEST: ");
  switch (payload.resource) {
       case LAMP:
         Serial.print("Lamp ");
         switch (payload.value) {
           case GET:
             Serial.println("get state.");
             sendLampState();
             break;
           case ON:
             Serial.println("turn on.");
             turnLampOn(true);
             break;
           case OFF:
             Serial.println("turn off.");
             turnLampOn(false);
             break;
           default:
             Serial.println("(Unknown action for lamp!).");
             break;
         }
         break;
       case KEYBOARD:
          Serial.println("Keyboard get.");
          sendKeyboardState();
          break;
       default:
         Serial.println("Unknown resource request!");
         break;
  }
}

void turnLampOn(bool on){
  if(on){
    Serial.println("Turning lamp on.");
    digitalWrite(A0, LOW);
    isLampOn = true;
  }else{
    Serial.println("Turning lamp off.");
    digitalWrite(A0, HIGH);
    isLampOn = false;
  }
}

void sendLampState(){
  char state;
  Serial.println("Sending lamp state.");
  if(isLampOn){
    state = ON;
  }else{
    state = OFF;
  }
  payload_t payload {millis(), LAMP, state};
  send(payload);
}

bool sendKeyboardState(){
  Serial.println("Sending keyboard state.");
  payload_t payload {millis(), KEYBOARD, lastKeyPressed};
  bool ok = send(payload);
  if(ok){
    lastKeySent = lastKeyPressed;
  }
  return ok;
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
