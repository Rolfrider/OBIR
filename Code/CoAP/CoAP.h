/*
Kod pod wpływem duzej inspiracji z reposytorium: https://github.com/hirotakaster/CoAP-simple-library
*/

#ifndef CoAP_h
#define CoAP_h

#include "Udp.h"
#define MAX_CALLBACK 10

#define HEADER_SIZE 4
#define OPTION_HEADER_SIZE 1
#define TOKEN_MAX_SIZE 0x0F
#define PAYLOAD_MARKER 0xFF
#define MAX_OPTION_NUM 10
#define BUF_MAX_SIZE 50
#define DEFAULT_PORT 5683 

/* Operatory bitowe
<< - przesuwa  bity w lewo zmiennej po lewej stronie o liczbe podana po prawej stronie.
Przyklad:  0011 0010 << 2 => 1100 1000
| - bitowy OR 
Przyklad: A = 0101, B = 1100  A | B = 1101 */
#define RESPONSE_CODE(class, detail) ((class << 5) | (detail))
// 0xFF - 0x, ze hex FF = 255 = 1111 1111
// & - bitowy AND
//Przyklad: A = 0101, B = 1100  A & B = 0100
// OPTION_DELTA sprawdza gdzie znajduje się delta jeśli <13 to w pierwszym bajcie
// jesli 13 < delta < 255 + 13 to delta = 13 
// delta > 255 + 13, delta = 14
#define OPTION_DELTA(v, n) (v < 13 ? (*n = (0xFF & v)) : (v <= 0xFF + 13 ? (*n = 13) : (*n = 14)))

#include "Arduino.h"


//Message type
typedef enum {
    CON = 0,
    NON = 1,
    ACK = 2,
    RESET = 3
} COAP_TYPE;

typedef enum {
  REQUEST = 0,
  SUCCESS_RESPONSE = 2,
  CLIENT_ERROR_RESPONSE = 4,
  SERVER_ERROR_RESPONSE = 5
}COAP_CLASS;

typedef enum {
    GET = 1,
    POST = 2,
    PUT = 3,
    DELETE = 4
} COAP_METHOD;

typedef enum {
    IF_MATCH = 1,
    URI_HOST = 3,
    E_TAG = 4,
    IF_NONE_MATCH = 5,
    URI_PORT = 7,
    LOCATION_PATH = 8,
    URI_PATH = 11,
    CONTENT_FORMAT = 12,
    MAX_AGE = 14,
    URI_QUERY = 15,
    ACCEPT = 17,
    LOCATION_QUERY = 20,
    PROXY_URI = 35,
    PROXY_SCHEME = 39
} COAP_OPTION_NUMBER;

typedef enum {
    NONE = -1,
    TEXT_PLAIN = 0,
    APPLICATION_LINK_FORMAT = 40,
    APPLICATION_XML = 41,
    APPLICATION_OCTET_STREAM = 42,
    APPLICATION_EXI = 47,
    APPLICATION_JSON = 50
} COAP_CONTENT_TYPE;

class CoAPOption {
    public:
    uint8_t number;
    uint8_t length;
    uint8_t *buffer;
};

class CoAPPacket {
    public:
    uint8_t type;
    uint8_t code;
    uint8_t *token;
    uint8_t tokenLen;
    uint8_t *payload;
    uint8_t payloadLen;
    uint16_t messageId;
    
    uint8_t optionNum;
    CoAPOption options[MAX_OPTION_NUM];
};

// Tworzy alias dla funkcji callback ktora bierze podane argumenty i nie zwraca nic (void)
typedef void (*callback)(CoAPPacket &, IPAddress, int);

class CoAPUri {
    private:
        String u[MAX_CALLBACK];
        callback c[MAX_CALLBACK];
    public:
        CoAPUri() {
          //Przypisuje do tablic puste wartosci
            for (int i = 0; i < MAX_CALLBACK; i++) {
                u[i] = "";
                c[i] = NULL;
            }
        };
        void add(callback call, String url) {
            for (int i = 0; i < MAX_CALLBACK; i++)
                if (c[i] != NULL && u[i].equals(url)) {
                    c[i] = call;
                    return ;
                }
            for (int i = 0; i < MAX_CALLBACK; i++) {
                if (c[i] == NULL) {
                    c[i] = call;
                    u[i] = url;
                    return;
                }
            }
        };
        callback find(String url) {
            for (int i = 0; i < MAX_CALLBACK; i++) if (c[i] != NULL && u[i].equals(url)) return c[i];
            return NULL;
        } ;
};



class CoAP
{
  private:
        UDP *_udp;
        CoAPUri uri;
        callback resp;
        int _port;

        uint16_t sendPacket(CoAPPacket &packet, IPAddress ip);
        uint16_t sendPacket(CoAPPacket &packet, IPAddress ip, int port);
        int parseOption(CoAPOption *option, uint16_t *running_delta, uint8_t **buf, size_t bufLen);

    public:
        CoAP(
            UDP& udp
        );
        bool start();
        bool start(int port);
        void response(callback c) { resp = c; }
        
        void server(callback c, String url) { uri.add(c, url); }
        uint16_t sendResponse(IPAddress ip, int port, uint16_t messageId);
        uint16_t sendResponse(IPAddress ip, int port, uint16_t messageId, char *payload);
        uint16_t sendResponse(IPAddress ip, int port, uint16_t messageId, char *payload, int payloadLen);
        uint16_t sendResponse(IPAddress ip, int port, uint16_t messageId, char *payload, int payloadLen, uint8_t responseCode, COAP_CONTENT_TYPE type, uint8_t *token, int tokenLen);
        
        uint16_t get(IPAddress ip, int port, char *url, COAP_TYPE type);
        uint16_t get(IPAddress ip, int port, char *url);
        
        uint16_t put(IPAddress ip, int port, char *url, char *payload, COAP_TYPE type);
        uint16_t put(IPAddress ip, int port, char *url, char *payload);
        uint16_t put(IPAddress ip, int port, char *url, char *payload, int payloadLen);


        uint16_t send(IPAddress ip, int port, char *url, COAP_TYPE type, COAP_METHOD method, uint8_t *token, uint8_t tokenLen, uint8_t *payload, uint32_t payloadLen);

        bool loop();
    
};

#endif