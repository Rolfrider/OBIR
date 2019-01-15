#include <CoAP.h>
#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <RF24Network.h>
#include <RF24.h>

//TODO: Powiadomienie obserwatora i counter++
//TODO: zwrocenia stanu klawiatury
//ETAG do zaiplementowania

const int OUR_CHANNEL = 65;
const uint16_t THIS_NODE_ID = 00;  // address of our node in Octal format ( 04,031, etc)
const uint16_t OTHER_NODE_ID = 01; // address of the other node in Octal format
const String wellKnownResp = "</light>;ct=0,</keyboard>;ct=0;rt=\"obs\",</statistics>;ct=0";

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};

String lightEndPoint = "light";
String keyboardEndPoint = "keyboard";
String wellKnownEndPoint = ".well-known/core";
String statisticsEndPoint = "statistics";

//for radiocommunication
struct payload_t
{ // structure of our payload
    unsigned long ms;
    unsigned int resource; //0-lamp 1-keyboard
    char value;            //g-get 0-off 1-on
};

// CoAP client response callback
void callback_response(CoAPPacket &packet, IPAddress ip, int port);

// CoAP server endpoint url callback
void callback_light(CoAPPacket &packet, IPAddress ip, int port);

void callback_wellKnown(CoAPPacket &packet, IPAddress ip, int port);

void callback_keyboard(CoAPPacket &packet, IPAddress ip, int port);

void callback_statistics(CoAPPacket &packet, IPAddress ip, int port);

bool send(payload_t payload);

void handlePayload(payload_t payload);

void getKeyboardState();

void setLampState(char state);

void getLampState();

struct Observer
{
    IPAddress ip;
    int port;
    uint8_t token[2];
    uint8_t tokenLen;
    uint8_t counter;
};

//for ETAG
struct Resource
{
    String value;
    uint8_t tag;
};

//payload resources
const int ALL = 0;
const int LAMP = 1;
const int KEYBOARD = 2;
//payload values
const char GETrf = 'g';
const char OFF = '0';
const char ON = '1';

Observer observers;

Resource resources[3];
// 0 for light
// 1 for statistics
// 2 keyboard
// UDP and CoAP class
EthernetUDP Udp;
CoAP coap(Udp);

RF24 radio(7, 8);           // nRF24L01(+) radio CE and CSN to 7th and 8th port
RF24Network network(radio); // network uses that radio

uint8_t numberOfReceivedMessages = 0;
uint8_t numberOfSentMessages = 0;

char lastKeyPressed = '0';
bool isLampOn = false;
bool firstLoop = true;

void updateStatisticsResource()
{
    resources[1].value = "Received = " + (String)numberOfReceivedMessages + " Sent = " + (String)numberOfSentMessages + " chanel = " + (String)(OUR_CHANNEL);
    resources[1].tag++;
}

// CoAP server endpoint URL
void callback_light(CoAPPacket &packet, IPAddress ip, int port)
{
    Serial.println(F("[Light] ON/OFF"));

    if (packet.code == GET)
    {
        Serial.println(F("IS GET"));
        for (int i = 0; i < sizeof(packet.options); i++)
        {
            if (packet.options[i].number == 0)
            {
                Serial.println(F("NOT THIS OPTION"));
                continue;
            }
            if (packet.options[i].number == 4)
            {
                Serial.println(F("OPTION ETAG"));

                if (*packet.options[i].buffer == resources[0].tag)
                {
                    coap.sendValidResponse(ip, port, packet.messageId, packet.token, packet.tokenLen);
                    return;
                }
                else
                {
                    coap.sendETagResponse(ip, port, packet.messageId, resources[0].value.c_str(), resources[0].tag, packet.token, packet.tokenLen);
                    //coap.sendResponse(ip, port, packet.messageId, resources[0].value.c_str());
                    return;
                }
            }
        }
        coap.sendETagResponse(ip, port, packet.messageId, resources[0].value.c_str(), resources[0].tag, packet.token, packet.tokenLen);
        //coap.sendResponse(ip, port, packet.messageId, resources[0].value.c_str());
    }
    if (packet.code == PUT)
    {
        Serial.println(F("PUT"));
        // DLA put
        // send response
        char p[packet.payloadLen + 1];
        memcpy(p, packet.payload, packet.payloadLen);
        p[packet.payloadLen] = NULL;

        String message(p);

        if (message.equals("0"))
        {
            //  kazac lampce zaktualizowac (wylaczy)
            setLampState(OFF);
        }
        else if (message.equals("1"))
        {
            // kazac lampce zaktualizowac(wlaczyc)
            setLampState(ON);
        }
    }
}

void callback_wellKnown(CoAPPacket &packet, IPAddress ip, int port)
{
    Serial.println(F("well know"));

    coap.sendResponse(ip, port, packet.messageId, wellKnownResp.c_str(), strlen(wellKnownResp.c_str()), CONTENT, APPLICATION_LINK_FORMAT, packet.token, packet.tokenLen);
}

void callback_statistics(CoAPPacket &packet, IPAddress ip, int port)
{
    Serial.println(F("Statistics"));

    // prosi o statystykiif (packet.code == GET)
    if (packet.code = GET)
    {
        Serial.println(F("IS GET"));
        for (int i = 0; i < sizeof(packet.options); i++)
        {
            if (packet.options[i].number == 0)
            {
                Serial.println(F("NOT THIS OPTION"));
                continue;
            }
            if (packet.options[i].number == 4)
            {
                Serial.println(F("OPTION ETAG"));

                if (*packet.options[i].buffer == resources[1].tag)
                {
                    coap.sendValidResponse(ip, port, packet.messageId, packet.token, packet.tokenLen);
                    return;
                }
                else
                {
                    coap.sendETagResponse(ip, port, packet.messageId, resources[1].value.c_str(), resources[1].tag, packet.token, packet.tokenLen);
                    //coap.sendResponse(ip, port, packet.messageId, resources[0].value.c_str());
                    return;
                }
            }
        }
        coap.sendETagResponse(ip, port, packet.messageId, resources[1].value.c_str(), resources[1].tag, packet.token, packet.tokenLen);
        //coap.sendResponse(ip, port, packet.messageId, resources[0].value.c_str());
    }
}

void callback_keyboard(CoAPPacket &packet, IPAddress ip, int port)
{
    Serial.println(F("Keyboard"));

    if (packet.code == GET)
    {
        Serial.println(F("GET"));
        for (int i = 0; i < sizeof(packet.options); i++)
        {
            if (packet.options[i].number == 0)
            {
                continue;
            }
            if (packet.options[i].number == 6)
            {
                Serial.println(F("OBSERVE"));
                if (*(packet.options[i].buffer) == 88)
                {
                    Serial.println(F("ADD OBSERVER"));
                    observers.ip = ip;
                    observers.port = port;
                    observers.counter = 2;
                    memcpy(observers.token, packet.token, packet.tokenLen);
                    //observers.token = packet.token;
                    observers.tokenLen = packet.tokenLen;
                    observers.counter++;
                    coap.notifyObserver(observers.ip, observers.port, observers.counter, resources[2].value.c_str(), strlen(resources[2].value.c_str()), observers.token, observers.tokenLen);
                    break;
                }
                else
                {
                    Serial.println(F("REMOVE OBSERVER"));
                    observers.counter = -1;
                    break;
                }
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println(F("# OBIR PROJECT: ARDUINO UNO #"));

    SPI.begin();
    radio.begin();
    network.begin(OUR_CHANNEL, THIS_NODE_ID);

    Ethernet.begin(mac);
    Serial.print(F("My IP address: "));
    for (byte thisByte = 0; thisByte < 4; thisByte++)
    {
        Serial.print(Ethernet.localIP()[thisByte], DEC);
        Serial.print(".");
    }
    Serial.println();

    // add server url endpoints.
    // can add multiple endpoint urls.
    Serial.println(F("Setup Callbacks"));
    coap.server(callback_light, lightEndPoint);
    coap.server(callback_keyboard, keyboardEndPoint);
    coap.server(callback_wellKnown, wellKnownEndPoint);
    coap.server(callback_statistics, statisticsEndPoint);

    resources[0].value = "off.";

    // start coap server/client
    coap.start();
}

void loop()
{
    network.update();
    coap.loop();

    while (network.available())
    {                             // is there anything ready for us?
        RF24NetworkHeader header; // if so, grab it and handle
        payload_t payload;
        network.read(header, &payload, sizeof(payload));
        Serial.print(F("Received packet at "));
        Serial.println(payload.ms);
        handlePayload(payload);

        Serial.print("Light - ");
        Serial.println(resources[0].value);
        Serial.print("Stats - ");
        Serial.println(resources[1].value);
        Serial.print("Keyboard - ");
        Serial.println(resources[2].value);
    }

    if (firstLoop)
    {
        getAllResOnStart();
        firstLoop = false;
    }
}

void getAllResOnStart()
{
    Serial.println(F("Sending initial request."));
    payload_t payload{millis(), ALL, GETrf};
    send(payload);
}

void getLampState()
{
    Serial.println(F("Sending lamp state request."));
    payload_t payload{millis(), LAMP, GETrf};
    send(payload);
}

void setLampState(char state)
{ // ON or OFF
    Serial.print(F("Sending lamp state change to "));
    if (state == ON)
    {
        Serial.println(F("on."));
        resources[0].value = "on";
    }
    else
    {
        Serial.println(F("off."));
        resources[0].value = "off";
    }
    resources[0].tag++;
    payload_t payload{millis(), LAMP, state};
    send(payload);
}

void getKeyboardState()
{
    Serial.println(F("Sending keyboard state request."));
    payload_t payload{millis(), KEYBOARD, GETrf};
    send(payload);
}

void handlePayload(payload_t payload)
{
    numberOfReceivedMessages++;
    updateStatisticsResource();
    Serial.print(F("MESSAGE: "));
    switch (payload.resource)
    {
    case LAMP:
        Serial.print(F("Lamp is "));
        switch (payload.value)
        {
        case ON:
            isLampOn = true;
            Serial.println(F("on."));
            resources[0].value = "on";
            break;
        case OFF:
            isLampOn = false;
            Serial.println(F("off."));
            resources[0].value = "off";
            break;
        default:
            Serial.println(F("(Unknown lamp state!)."));
            break;
        }
        resources[0].tag++;
        break;
    case KEYBOARD:
        lastKeyPressed = payload.value;
        Serial.print(F("Keyboard's last key pressed is \""));
        Serial.print(payload.value);
        Serial.println(F("\""));

        resources[2].tag++;
        resources[2].value = lastKeyPressed;
        // powiadom obserwatora
        if (observers.counter != -1)
        {
            observers.counter++;
            coap.notifyObserver(observers.ip, observers.port, observers.counter, resources[2].value.c_str(), strlen(resources[2].value.c_str()), observers.token, observers.tokenLen);
            // coap.sendResponse(observers.ip, observers.port, 1, lastKeyPressed, strlen(lastKeyPressed),
            //                   CONTENT, TEXT_PLAIN, observers.token, sizeof(observers.token));
        }
        break;
    default:
        Serial.println(F("Unknown message"));
        break;
    }
}

bool send(payload_t payload)
{
    numberOfSentMessages++;
    updateStatisticsResource();
    Serial.print(F("Sending..."));
    RF24NetworkHeader header(OTHER_NODE_ID);
    bool ok = network.write(header, &payload, sizeof(payload));
    if (ok)
    {
        Serial.println(F("ok."));
        return true;
    }
    else
    {
        Serial.println(F("failed."));
        return false;
    }
}
