#include "Arduino.h"
#include "CoAP.h"

#define LOGGING

CoAP::CoAP(UDP &udp)
{
    this->_udp = &udp;
}

bool CoAP::start()
{
    this->start(DEFAULT_PORT);
    return true;
}

bool CoAP::start(int port)
{
    // begin zaczyna słuchać na podanym porcie, zwraca 1 jesli operacja sie powiedzie
    return this->_udp->begin(port) == 1 ? true : false;
}

uint16_t CoAP::sendPacket(CoAPPacket &packet, IPAddress ip)
{
    return this->sendPacket(packet, ip, DEFAULT_PORT);
}

uint16_t CoAP::sendPacket(CoAPPacket &packet, IPAddress ip, int port)
{
    uint8_t buffer[BUF_MAX_SIZE];
    uint8_t *p = buffer; // pakiet wskazuje na poczatek bufforu
    uint16_t running_delta = 0;
    uint16_t packetSize = 0;
    //|     Code       |
    // nagłowek głowny                VER | T  | TKL  | class | detail |      Message ID     | Token     |
    //                                 00 | 00 | 0000 |  000  |  00000 | 0000 0000 0000 0000 |
    *p = 0x01 << 6; // 01 | 00 | 0000 |  000  |  00000 | 0000 0000 0000 0000 |
    // Teraz najpierw AND ktory da w swoim wyniku dwie niewiadome(xx) na dwoch ostatnich miejscach a nastepnie przesunie je o 4 w lewo
    // i OR z istniejacym naglowkiem
    *p |= (packet.type & 0x03) << 4; // 01 | xx | 0000 |
    // *p++ inkrementacja wskaźnika
    *p++ |= (packet.tokenLen & 0x0F); //01 | xx | yyyy |
    *p++ = packet.code;               // 01 | xx | yyyy |  ccc  |  ccccc |
    // Message ID jest 16 bitowe dlatego najpierw wpisuje 8 pierszych bitow
    *p++ = (packet.messageId >> 8); // 01 | xx | yyyy |  ccc  |  ccccc | mmmm mmmm
    // Kolejne 8 bitow uzyskuje przez AND z 0xFF = 1111 1111
    *p++ = (packet.messageId & 0xFF); //01 | xx | yyyy |  ccc  |  ccccc | mmmm mmmm nnnn nnnn |

    p = buffer + HEADER_SIZE;

    packetSize += HEADER_SIZE;

    // token
    if (packet.token != NULL && packet.tokenLen <= TOKEN_MAX_SIZE)
    {
        memcpy(p, packet.token, packet.tokenLen);
        //01 | xx | yyyy |  ccc  |  ccccc | mmmm mmmm nnnn nnnn | tt..tt   |
        p += packet.tokenLen;
        packetSize += packet.tokenLen;
    }

    // option
    for (int i = 0; i < packet.optionNum; i++)
    {
        uint32_t optDelta;
        uint8_t len, delta;

        if (packetSize + 5 + packet.options[i].length >= BUF_MAX_SIZE)
        {
            // czy nagłowek opcji i jej wartość zmieszcza sie w buforze
            return 0;
        }

        optDelta = packet.options[i].number - running_delta;
        OPTION_DELTA(optDelta, &delta);                         // przypiszemy do delta nasz delte z przedziału (1, 14)
        OPTION_DELTA((uint32_t)packet.options[i].length, &len); // tak samo sprawdzamy dlugosc opcji

        // tutaj przesuwamy delte o 4 w lewo bo w bloku opcji pierwsze 4 bity oznaczają delte a kolejne 4 długość
        // dlatego po OR z len otrzymamy  dddd llll
        *p++ = (delta << 4 | len); // tutaj jeszcze AND był z 0xFF ale chyba nie potrzebny
        packetSize++;              // zwiekszamy rozmiar
        if (delta == 13)
        {
            *p++ = (optDelta - 13); // przypisujemy 8 ostatnich bitow do pakietu
            packetSize++;           // zwiekszamy rozmiar o 1 bajt
        }
        else if (delta == 14)
        {
            *p++ = ((optDelta - 269) >> 8);   // zapisujemy najpierw pierwszy bajt
            *p++ = (0xFF & (optDelta - 269)); // a nastepnie kolejny i obcinamy tylko 8 ostatnich bitow
            packetSize += 2;                  // zwiekszamy o 2
        }
        if (len == 13)
        { // to samo z dlugoscia pakietu
            *p++ = (packet.options[i].length - 13);
            packetSize++;
        }
        else if (len == 14)
        {
            *p++ = (packet.options[i].length >> 8);
            *p++ = (0xFF & (packet.options[i].length - 269));
            packetSize += 2;
        }

        memcpy(p, packet.options[i].buffer, packet.options[i].length); // kopjujemy wartosc opcji do pakietu
        p += packet.options[i].length;                                 // zwiekszamy pakiet o dlugosc wartosci opcji
        packetSize += packet.options[i].length;
        running_delta = packet.options[i].number; // aktualizujemy delte
    }

    // payload

    if (packet.payloadLen > 0)
    { // sprawdzam czy pakiet nie jest pusty
        if ((packetSize + 1 + packet.payloadLen) >= BUF_MAX_SIZE)
        { // czy sie zmiesci (+1 na marker)
            return 0;
        }

        *p++ = 0xFF; // marker
        memcpy(p, packet.payload, packet.payloadLen);
        packetSize += 1 + packet.payloadLen;
    }

    // wysylanie

    _udp->beginPacket(ip, port);
    _udp->write(buffer, packetSize);
    _udp->endPacket();

    return packet.messageId;
}

uint16_t CoAP::send(IPAddress ip, int port, char *url, COAP_TYPE type, COAP_METHOD method,
                    uint8_t *token, uint8_t tokenLen, uint8_t *payload, uint32_t payloadLen)
{
    // tworzenie pakietu
    CoAPPacket packet;
    packet.type = type;
    packet.code = method;
    packet.token = token;
    packet.tokenLen = tokenLen;
    packet.payload = payload;
    packet.payloadLen = payloadLen;
    packet.optionNum = 0;
    packet.messageId = rand();

    // dodajemy Opcje URI_HOST
    String ipaddress = String(ip[0]) + String(".") + String(ip[1]) + String(".") + String(ip[2]) + String(".") + String(ip[3]);
    packet.options[packet.optionNum].buffer = (uint8_t *)ipaddress.c_str();
    packet.options[packet.optionNum].length = ipaddress.length();
    packet.options[packet.optionNum].number = URI_HOST;
    packet.optionNum++;

    int idx = 0;
    for (int i = 0; i < strlen(url); i++)
    {
        if (url[i] == '/')
        {
            packet.options[packet.optionNum].buffer = (uint8_t *)(url + idx); // zapisuje wskaxnik
            packet.options[packet.optionNum].length = i - idx;                // id zasobu
            packet.options[packet.optionNum].number = URI_PATH;
            packet.optionNum++;
            idx = i + 1;
        }
    }
    // ostatnie id
    if (idx <= strlen(url))
    {
        packet.options[packet.optionNum].buffer = (uint8_t *)(url + idx);
        packet.options[packet.optionNum].length = strlen(url) - idx;
        packet.options[packet.optionNum].number = URI_PATH;
        packet.optionNum++;
    }

    // wysłanie
    return this->sendPacket(packet, ip, port);
}

int CoAP::parseOption(CoAPOption *option, uint16_t *running_delta, uint8_t **buf, size_t bufLen)
{
    uint8_t *p = *buf; // wskaznik do wskaznika bufora
    uint8_t headLen = 1;
    uint16_t len, delta;

    if (bufLen < headLen)
        return -1; // jesli bufor ma mniejsza dlugosc niz 1 to znaczy ze jest pusty

    //  bierze pierwsze 8 bitow z buffora i AND 11110000 a potem przesuwa 4 w lewo => 0000 xxxx
    delta = (p[0] & 0xF0) >> 4; // tak uzyskuje delte
    len = p[0] & 0x0F;          // dlugosc to analogicznie 4 bity po prawej stronie

    if (delta == 13)
    {
        headLen++;
        if (bufLen < headLen)
            return -1;
        delta = p[1] + 13;
        p++;
    }
    else if (delta == 14)
    {
        headLen += 2;
        if (bufLen < headLen)
            return -1;
        delta = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (delta == 15)
        return -1;

    if (len == 13)
    {
        headLen++;
        if (bufLen < headLen)
            return -1;
        len = p[1] + 13;
        p++;
    }
    else if (len == 14)
    {
        headLen += 2;
        if (bufLen < headLen)
            return -1;
        len = ((p[1] << 8) | p[2]) + 269;
        p += 2;
    }
    else if (len == 15)
        return -1;

    if ((p + 1 + len) > (*buf + bufLen))
        return -1;
    option->number = delta + *running_delta;
    option->buffer = p + 1;
    option->length = len;
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

bool CoAP::loop()
{
    uint8_t buffer[BUF_MAX_SIZE];
    int32_t packetLen = _udp->parsePacket(); // zwraca rozmiar pakietu jesli nie ma to 0

    while (packetLen > 0)
    {
        // czyta caly lub dlugosc bufforu
        packetLen = _udp->read(buffer, packetLen >= BUF_MAX_SIZE ? BUF_MAX_SIZE : packetLen);

        CoAPPacket packet;

        // parse coap packet header
        if (packetLen < HEADER_SIZE || (((buffer[0] & 0xC0) >> 6) != 1)) // sprawdza czy
        //header zostal juz przczytany i czy wersja sie zgadza
        {
            packetLen = _udp->parsePacket();
            continue;
        }

        packet.type = (buffer[0] & 0x30) >> 4; // bierze dwa bity typu wiadomosci
        packet.tokenLen = buffer[0] & 0x0F;    // dlugosc tokena
        packet.code = buffer[1];
        packet.messageId = 0xFF00 & (buffer[2] << 8); // pierwsz 8 bitow messageId
        packet.messageId |= 0x00FF & buffer[3];       // kolejne 8 bitow messageId

        if (packet.tokenLen == 0)
            packet.token = NULL;
        else if (packet.tokenLen <= 8)
            packet.token = buffer + 4; // daje wskaźnik na buffer[4]
        else
        {
            packetLen = _udp->parsePacket();
            continue;
        }

        // parse packet options/payload
        if (HEADER_SIZE + packet.tokenLen < packetLen) // czy wiadomosc zawier cos ponad header i token
        {
            int optionIndex = 0;
            uint16_t delta = 0;
            uint8_t *end = buffer + packetLen;
            uint8_t *p = buffer + HEADER_SIZE + packet.tokenLen;
            while (optionIndex < MAX_OPTION_NUM && *p != 0xFF && p < end) // dopki nie przeczyta wszystkich
            // opcji lub dotrze do markera lub konca pakietu
            {
                packet.options[optionIndex];
                if (0 != parseOption(&packet.options[optionIndex], &delta, &p, end - p))
                    return false;
                optionIndex++;
            }
            packet.optionNum = optionIndex;

            if (p + 1 < end && *p == 0xFF) // czy jest co najmniej bit payloadu
            {
                packet.payload = p + 1;
                packet.payloadLen = end - (p + 1);
            }
            else
            {
                packet.payload = NULL;
                packet.payloadLen = 0;
            }
        }

        if (packet.type == ACK)
        {
            // call response function
            resp(packet, _udp->remoteIP(), _udp->remotePort());
        }
        else
        {

            String url = "";
            // call endpoint url function
            for (int i = 0; i < packet.optionNum; i++)
            {
                if (packet.options[i].number == URI_PATH && packet.options[i].length > 0)
                {
                    char urlname[packet.options[i].length + 1];
                    memcpy(urlname, packet.options[i].buffer, packet.options[i].length);
                    urlname[packet.options[i].length] = NULL;
                    if (url.length() > 0)
                        url += "/";
                    url += urlname;
                }
            }

            if (!uri.find(url))
            {
                // nie moze znalec URI zwraca NOT_FOUNT
                sendResponse(_udp->remoteIP(), _udp->remotePort(), packet.messageId, NULL, 0,
                             NOT_FOUNT, NONE, NULL, 0);
            }
            else
            {
                uri.find(url)(packet, _udp->remoteIP(), _udp->remotePort());
            }
        }

        // next packet
        packetLen = _udp->parsePacket();
    }

    return true;
}

uint16_t CoAP::sendValidResponse(IPAddress ip, int port, uint16_t messageId, uint8_t *token, int tokenLen)
{
    CoAPPacket packet;

    packet.type = ACK;
    packet.code = VALID;
    packet.token = token;
    packet.tokenLen = tokenLen;
    packet.payload = NULL;
    packet.payloadLen = 0;
    packet.optionNum = 0;
    packet.messageId = messageId;

    return this->sendPacket(packet, ip, port);
}

uint16_t CoAP::sendETagResponse(IPAddress ip, int port, uint16_t messageId, char *payload, uint8_t tag, uint8_t *token, int tokenLen)
{
    CoAPPacket packet;

    packet.type = ACK;
    packet.code = CONTENT;
    packet.token = token;
    packet.tokenLen = tokenLen;
    packet.payload = (uint8_t *)payload;
    packet.payloadLen = strlen(payload);
    packet.optionNum = 0;
    packet.messageId = messageId;

    // if more options?

    packet.options[packet.optionNum].buffer = &tag;
    packet.options[packet.optionNum].length = 2;
    packet.options[packet.optionNum].number = E_TAG;
    packet.optionNum++;
    char optionBuffer[2];
    optionBuffer[0] = ((uint16_t)TEXT_PLAIN & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)TEXT_PLAIN & 0x00FF);
    packet.options[packet.optionNum].buffer = (uint8_t *)optionBuffer;
    packet.options[packet.optionNum].length = 2;
    packet.options[packet.optionNum].number = CONTENT_FORMAT;
    packet.optionNum++;
    // Etag

    return this->sendPacket(packet, ip, port);
}

uint16_t CoAP::sendResponse(IPAddress ip, int port, uint16_t messageId)
{
    this->sendResponse(ip, port, messageId, NULL, 0, CONTENT, TEXT_PLAIN, NULL, 0);
}

uint16_t CoAP::sendResponse(IPAddress ip, int port, uint16_t messageId, char *payload)
{
    this->sendResponse(ip, port, messageId, payload, strlen(payload), CONTENT, TEXT_PLAIN, NULL, 0);
}

uint16_t CoAP::sendResponse(IPAddress ip, int port, uint16_t messageId, char *payload, int payloadLen)
{
    this->sendResponse(ip, port, messageId, payload, payloadLen, CONTENT, TEXT_PLAIN, NULL, 0);
}

uint16_t CoAP::notifyObserver(IPAddress ip, int port, uint8_t obs, char *payload, int payloadLen, uint8_t *token, uint8_t tokenLen)
{
    // make packet
    CoAPPacket packet;

    packet.type = NON;
    packet.code = CONTENT;
    packet.token = token;
    packet.tokenLen = tokenLen;
    packet.payload = (uint8_t *)payload;
    packet.payloadLen = payloadLen;
    packet.optionNum = 0;
    packet.messageId = NULL;

    // if more options?
    packet.options[packet.optionNum].buffer = &obs;
    packet.options[packet.optionNum].length = 1;
    packet.options[packet.optionNum].number = OBSERVE;
    packet.optionNum++;
    char optionBuffer[2];
    optionBuffer[0] = ((uint16_t)TEXT_PLAIN & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)TEXT_PLAIN & 0x00FF);
    packet.options[packet.optionNum].buffer = (uint8_t *)optionBuffer;
    packet.options[packet.optionNum].length = 2;
    packet.options[packet.optionNum].number = CONTENT_FORMAT;
    packet.optionNum++;

    return this->sendPacket(packet, ip, port);
}

uint16_t CoAP::sendResponse(IPAddress ip, int port, uint16_t messageId, char *payload, int payloadLen,
                            COAP_RESPONSE_CODE responseCode, COAP_CONTENT_TYPE type, uint8_t *token, int tokenLen)
{
    // make packet
    CoAPPacket packet;

    packet.type = ACK;
    packet.code = responseCode;
    packet.token = token;
    packet.tokenLen = tokenLen;
    packet.payload = (uint8_t *)payload;
    packet.payloadLen = payloadLen;
    packet.optionNum = 0;
    packet.messageId = messageId;

    // if more options?
    char optionBuffer[2];
    optionBuffer[0] = ((uint16_t)type & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t)type & 0x00FF);
    packet.options[packet.optionNum].buffer = (uint8_t *)optionBuffer;
    packet.options[packet.optionNum].length = 2;
    packet.options[packet.optionNum].number = CONTENT_FORMAT;
    packet.optionNum++;

    return this->sendPacket(packet, ip, port);
}