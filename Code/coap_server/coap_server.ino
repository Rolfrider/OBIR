#include <CoAP.h>
#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

//TODO: Powiadomienie obserwatora i counter++
// ETAG do zaiplementowania

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};

String lightEndPoint = "light";
String keyboardEndPoint = "keyboard";
String wellKnownEndPoint = ".well- known/core";
String statisticsEndPoint = "statistics";

// CoAP client response callback
void callback_response(CoAPPacket &packet, IPAddress ip, int port);

// CoAP server endpoint url callback
void callback_light(CoAPPacket &packet, IPAddress ip, int port);

void callback_wellKnown(CoAPPacket &packet, IPAddress ip, int port);

void callback_keyboard(CoAPPacket &packet, IPAddress ip, int port);

void callback_statistics(CoAPPacket &packet, IPAddress ip, int port);

struct Observer
{
    IPAddress ip;
    int port;
    uint8_t token;
    int counter;
};

//for ETAG
struct Resource
{
    String name;
    String value;
    uint8_t tag;
};

Observer observers[5];

Resource resources[3];
// UDP and CoAP class
EthernetUDP Udp;
CoAP coap(Udp);

bool LEDSTATE;

// CoAP server endpoint URL
void callback_light(CoAPPacket &packet, IPAddress ip, int port)
{
    Serial.println("[Light] ON/OFF");

    if (packet.code == GET)
    {
        // zapytać mini o lampke jak jest

        coap.sendResponse(ip, port, packet.messageId, "" /* Wartośc lampki*/);
    }
    if (packet.code == PUT)
    {
        // DLA put lub post
        // send response
        char p[packet.payloadLen + 1];
        memcpy(p, packet.payload, packet.payloadLen);
        p[packet.payloadLen] = NULL;

        String message(p);

        if (message.equals("0"))
            LEDSTATE = false;
        else if (message.equals("1"))
            LEDSTATE = true;

        if (LEDSTATE)
        {
            // kazac lampce zaktualizowac(wlaczyc)
            coap.sendResponse(ip, port, packet.messageId, "1");
        }
        else
        {
            //  kazac lampce zaktualizowac (wylaczy)
            coap.sendResponse(ip, port, packet.messageId, "0");
        }
    }
}

void callback_wellKnown(CoAPPacket &packet, IPAddress ip, int port)
{
    Serial.println("well know");

    // link-format
    String payload = wellKnownEndPoint + " informations about endpoints\n" +
                     lightEndPoint + " Can light the lamp on(1) or off(0) with PUT method, or check the lamp state with get\n" +
                     keyboardEndPoint + " Response to GET calls to be or not observed\n" +
                     statisticsEndPoint + " Set of radio connection statistics ";

    Serial.println("Size of response payload" + sizeof(payload));
    // todo: new sendResponse with link-format
    coap.sendResponse(ip, port, packet.messageId, payload.c_str());
}

void callback_statistics(CoAPPacket &packet, IPAddress ip, int port)
{
    Serial.println("Statistics");

    // prosi o statystyki

    coap.sendResponse(ip, port, packet.messageId, " statistics placeholder");
}

void callback_keyboard(CoAPPacket &packet, IPAddress ip, int port)
{
    Serial.println("Keyboard");

    if (packet.code == GET)
    {
        for (int i = 0; i < sizeof(packet.options); i++)
        {
            if (packet.options[i].number == 0)
            {
                continue;
            }
            if (packet.options[i].number == 6)
            {
                if (packet.options[i].buffer == 0)
                {
                    // dodaj obserwatora
                    addObserver(ip, port, packet.token);
                    break;
                }
                else
                {
                    // usun obserwatora
                    removeObserver(ip, port);
                    break;
                }
            }
        }
    }
}

void addObserver(IPAddress ip, int port, uint8_t token)
{
    for (int i = 0; i < sizeof(observers); i++)
    {
        // Observer jest incjalizowany z {0,0,0}
        if (observers[i].port == 0 || observers[i].counter == -1)
        {
            observers[i].ip = ip;
            observers[i].port = port;
            observers[i].counter = 0;
            observers[i].token = token;
        }
    }
    observers[0].ip = ip;
    observers[0].port = port;
    observers[0].counter = 0;
    observers[0].token = token;
}

void removeObserver(IPAddress ip, int port)
{
    for (int i = 0; i < sizeof(observers); i++)
    {
        if (observers[i].ip == ip)
        {
            observers[i].counter = -1;
        }
    }
}

void setup()
{

    Serial.begin(9600);

    Ethernet.begin(mac);
    Serial.print("My IP address: ");
    for (byte thisByte = 0; thisByte < 4; thisByte++)
    {
        Serial.print(Ethernet.localIP()[thisByte], DEC);
        Serial.print(".");
    }
    Serial.println();

    LEDSTATE = true;

    // add server url endpoints.
    // can add multiple endpoint urls.
    Serial.println("Setup Callbacks");
    coap.server(callback_light, lightEndPoint);
    coap.server(callback_keyboard, keyboardEndPoint);
    coap.server(callback_wellKnown, wellKnownEndPoint);
    coap.server(callback_statistics, statisticsEndPoint);

    // start coap server/client
    coap.start();
}

void loop()
{
    Serial.println("Send Request");

    delay(1000);
    coap.loop();
}
