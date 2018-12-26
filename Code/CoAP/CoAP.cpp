#include "Arduino.h"
#include "CoAP.h"

#define LOGGING

CoAP::CoAP( UDP& udp){
    this->_udp = &udp;
}

bool CoAP::start(){
    this->start(DEFAULT_PORT);
    return true;
}

bool CoAP::start(int port){
    // begin zaczyna słuchać na podanym porcie, zwraca 1 jesli operacja sie powiedzie
    return this->_udp->begin(port) == 1 ? true : false;
}

uint16_t CoAP::sendPacket(CoAPPacket &packet, IPAddress ip){
    return this->sendPacket(packet, ip, DEFAULT_PORT);
}

uint16_t CoAP::sendPacket(CoAPPacket &packet, IPAddress ip, int port){
    uint8_t buffer[BUF_MAX_SIZE];
    uint8_t *p = buffer; // pakiet wskazuje na poczatek bufferu
    uint16_t running_delta = 0;
    uint16_t packetSize = 0;
                                                    //|     Code       |
    // nagłowek głowny                VER | T  | TKL  | class | detail |      Message ID     | Token     |
    //                                 00 | 00 | 0000 |  000  |  00000 | 0000 0000 0000 0000 |
    *p = 0x01 << 6;                 // 01 | 00 | 0000 |  000  |  00000 | 0000 0000 0000 0000 |
    // Teraz najpierw AND ktory da w swoim wyniku dwie niewiadome(xx) na dwoch ostatnich miejscach a nastepnie przesunie je o 4 w lewo
    // i OR z istniejacym naglowkiem
    *p |= (packet.type & 0x03) << 4;// 01 | xx | 0000 |
    // *p++ inkrementacja wskaźnika
    *p++ |= (packet.tokenLen & 0x0F);//01 | xx | yyyy |
    *p++ = packet.code;             // 01 | xx | yyyy |  ccc  |  ccccc |
    // Message ID jest 16 bitowe dlatego najpierw wpisuje 8 pierszych bitow
    *p++ = (packet.messageId >> 8); // 01 | xx | yyyy |  ccc  |  ccccc | mmmm mmmm 
    // Kolejne 8 bitow uzyskuje przez AND z 0xFF = 1111 1111
    *p++ = (packet.messageId & 0xFF);//01 | xx | yyyy |  ccc  |  ccccc | mmmm mmmm nnnn nnnn |

    p = buffer + HEADER_SIZE;

    packetSize += HEADER_SIZE;

    // token
    if(packet.token != NULL && packet.tokenLen <= TOKEN_MAX_SIZE){ 
        memcpy(p, packet.token, packet.tokenLen); 
                                     //01 | xx | yyyy |  ccc  |  ccccc | mmmm mmmm nnnn nnnn | tt..tt   |
        p += packet.tokenLen;
        packetSize += packet.tokenLen;

    }

    // option
    for (int i = 0; i < packet.optionNum; i++)  {
        uint32_t optDelta;
        uint8_t len, delta;

        if (packetSize + 5 + packet.options[i].length >= BUF_MAX_SIZE) {
            // czy nagłowek opcji i jej wartość zmieszcza sie w buforze
            return 0;
        }

        optDelta = packet.options[i].number - running_delta;
        OPTION_DELTA(optDelta, &delta);// przypiszemy do delta nasz delte z przedziału (1, 14)
        OPTION_DELTA((uint32_t)packet.options[i].length, &len);// tak samo sprawdzamy dlugosc opcji

        // tutaj przesuwamy delte o 4 w lewo bo w bloku opcji pierwsze 4 bity oznaczają delte a kolejne 4 długość
        // dlatego po OR z len otrzymamy  dddd llll
        *p++ = (delta << 4 | len); // tutaj jeszcze AND był z 0xFF ale chyba nie potrzebny
        packetSize++; // zwiekszamy rozmiar
        if (delta == 13) {
            *p++ = (optDelta - 13);// przypisujemy 8 ostatnich bitow do pakietu
            packetSize++; // zwiekszamy rozmiar o 1 bajt
        } else if (delta == 14) {
            *p++ = ((optDelta - 269) >> 8); // zapisujemy najpierw pierwszy bajt
            *p++ = (0xFF & (optDelta - 269));// a nastepnie kolejny i obcinamy tylko 8 ostatnich bitow
            packetSize+=2; // zwiekszamy o 2 
        } if (len == 13) { // to samo z dlugoscia pakietu
            *p++ = (packet.options[i].length - 13);
            packetSize++;
        } else if (len == 14) {
            *p++ = (packet.options[i].length >> 8);
            *p++ = (0xFF & (packet.options[i].length - 269));
            packetSize+=2;
        }

        memcpy(p, packet.options[i].buffer, packet.options[i].length); // kopjujemy wartosc opcji do pakietu
        p += packet.options[i].length; // zwiekszamy pakiet o dlugosc wartosci opcji
        packetSize += packet.options[i].length;
        running_delta = packet.options[i].number; // aktualizujemy delte 


    }

    // payload

    if(packet.payloadLen > 0 ){ // sprawdzam czy pakiet nie jest pusty
        if((packetSize + 1 + packet.payloadLen) >= BUF_MAX_SIZE){ // czy sie zmiesci (+1 na marker)
            return 0;
        }

        *p++ = 0xFF;// marker
        memcpy(p, packet.payload, packet.payloadLen);
        packetSize += 1 + packet.payloadLen;
    }

    // wysylanie

    _udp -> beginPacket(ip, port);
    _udp -> write(buffer, packetSize);
    _udp -> endPacket();

    return packet.messageId;


}

