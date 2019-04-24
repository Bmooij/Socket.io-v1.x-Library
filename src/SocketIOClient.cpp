/*
socket.io-arduino-client: a Socket.IO client for the Arduino
Based on the Kevin Rohling WebSocketClient & Bill Roy Socket.io Lbrary
Copyright 2015 Florent Vidal
Supports Socket.io v1.x
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:
The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/
#include <SocketIOClient.h>

void SocketIOClient::begin(const char* host, unsigned int port, const char* root_ca) {
    _host = host;
    _port = port;
    _secure = root_ca == nullptr;
    _root_ca = root_ca;
}

bool SocketIOClient::connect(const char* host, unsigned int port, const char* root_ca) {
    begin(host, port);
    return clientConnected();
}


bool SocketIOClient::connected() {
    return client.connected();
}

void SocketIOClient::disconnect() {
    client.stop();
}

// find the nth colon starting from dataptr
void SocketIOClient::findColon(char which) {
    while (*dataptr) {
        if (*dataptr == ':') {
            if (--which <= 0) return;
        }
        ++dataptr;
    }
}

// terminate command at dataptr at closing double quote
void SocketIOClient::terminateCommand(void) {
    dataptr[strlen(dataptr) - 3] = 0;
}

void SocketIOClient::parser(int index) {
    String payload = "";
    int sizemsg = databuffer[index + 1];  // 0-125 byte, index ok        Fix provide by Galilei11. Thanks
    if (databuffer[index + 1] > 125) {
        sizemsg = databuffer[index + 2];  // 126-255 byte
        index += 1;                       // index correction to start
    }
    for (int i = index + 2; i < index + sizemsg + 2; i++)
        payload += (char)databuffer[i];

	size_t length = payload.length();
	engineIOmessageType_t eType = (engineIOmessageType_t) payload[0];
    switch (eType) {
        case eIOtype_PING:
            DEBUG_WEBSOCKETS("[socketIO] Ping received - Sending Pong\n");
            sendPong();
            break;

        case eIOtype_PONG:
            DEBUG_WEBSOCKETS("[socketIO] Pong received - All good\n");
            break;

        case eIOtype_MESSAGE:
			if(length < 2) {
				break;
			}
			socketIOmessageType_t ioType = (socketIOmessageType_t) payload[1];
			uint8_t * data = (uint8_t *)&payload[2];
			size_t lData = length - 2;
			socketIOPacket_t packet;
			switch(ioType) {
				case sIOtype_EVENT:
					DEBUG_WEBSOCKETS("[socketIO] get event (%d): %s\n", lData, data);
					packet = parse(std::string((char *)data));
					triggerEvent(packet);
					break;
				case sIOtype_CONNECT:
					DEBUG_WEBSOCKETS("[socketIO] connected\n");
					packet.event = "connect";
					triggerEvent(packet);
					break;
				case sIOtype_DISCONNECT:
					DEBUG_WEBSOCKETS("[socketIO] disconnected\n");
					packet.event = "disconnect";
					triggerEvent(packet);
					break;
				case sIOtype_ACK:
					DEBUG_WEBSOCKETS("[socketIO] get ack (%d): %s\n", lData, data);
					packet = parse(std::string((char *)data));
					triggerAck(packet);
					break;
				case sIOtype_ERROR:
				case sIOtype_BINARY_EVENT:
				case sIOtype_BINARY_ACK:
				default:
					DEBUG_WEBSOCKETS("[socketIO] Socket.IO Message Type %c (%02X) is not implemented\n", ioType, ioType);
					DEBUG_WEBSOCKETS("[socketIO] get text: %s\n", payload);
					break;
			}
    }
}

bool SocketIOClient::clientConnected(void) {
    if (!connected()) {
        if (_secure) {
            client.setCACert(_root_ca);
        }
        if (!client.connect(_host, _port)) return false;
        if(!doHandshake()) return false;
        lastPing = millis();
    }
    return true;
}

void SocketIOClient::loop() {
    int index = -1;
    int index2 = -1;
    String tmp = "";
    *databuffer = 0;

    if (!clientConnected()) return;
    
    if (millis() - lastPing >= pingInterval) {
        sendPing();
        lastPing = millis();
    }

    if (!client.available()) return;

    while (client.available()) {
        readLine();
        tmp = databuffer;
        // Serial.println(databuffer);
        dataptr = databuffer;
        index = tmp.indexOf((char)129);  //129 DEC = 0x81 HEX = sent for proper communication
        index2 = tmp.indexOf((char)129, index + 1);
        if (index != -1) {
            parser(index);
        }
        if (index2 != -1) {
            parser(index2);
        }
    }
}

bool SocketIOClient::waitForInput(void) {
    unsigned long now = millis();
    while (!client.available() && ((millis() - now) < 30000UL)) {
        ;
    }
    return client.available();
}

void SocketIOClient::eatHeader(void) {
    while (client.available()) {  // consume the header
        readLine();
        if (strlen(databuffer) == 0) break;
    }
}

bool SocketIOClient::doHandshake() {
    uint8_t handshakeBuffer[512];
    int size;

    size = sprintf((char *)handshakeBuffer,
        "GET /socket.io/1/?transport=polling&b64=true HTTP/1.1\r\n" \
        "Host: %s\r\n" \
        "Origin: Arduino\r\n" \
        "\r\n"
    , _host);
    client.write(handshakeBuffer, size);

    if (!waitForInput()) return false;
    

    // check for happy "HTTP/1.1 200" response
    readLine();
    if (atoi(&databuffer[9]) != 200) {
        while (client.available()) readLine();
        client.stop();
        return false;
    }
    eatHeader();
    readLine();
    String tmp = databuffer;

    int sidIndex = tmp.indexOf("sid\":\"") + 6;
    int sidEndIndex = tmp.indexOf("\"", sidIndex);

    String sid = tmp.substring(sidIndex, sidEndIndex);

    int pingIntervalIndex = tmp.indexOf("pingInterval\":") + 14;
    int pingIntervalEndIndex = tmp.indexOf(",", pingIntervalIndex);

    String pingIntervalStr = tmp.substring(pingIntervalIndex, pingIntervalEndIndex);

    pingInterval = pingIntervalStr.toInt();

    while (client.available()) readLine();
    client.stop();
    delay(1000);


    if (!client.connect(_host, _port)) return false;

    size = sprintf((char *)handshakeBuffer,
        "GET /socket.io/1/websocket/?transport=websocket&b64=true&sid=%s HTTP/1.1\r\n" \
        "Host: %s\r\n" \
        "Sec-WebSocket-Version: 13\r\n" \
        "Origin: ArduinoSocketIOClient\r\n" \
        "Sec-WebSocket-Extensions: permessage-deflate\r\n" \
        "Sec-WebSocket-Key: IAMVERYEXCITEDESP32FTW==\r\n" \
        "Cookie: io=%s\r\n" \
        "Connection: Upgrade\r\n" \
        "Upgrade: websocket\r\n" \
        "\r\n"
    , sid.c_str(), _host, sid.c_str());
    client.write(handshakeBuffer, size);

    if (!waitForInput()) return false;
    readLine();
    if (atoi(&databuffer[9]) != 101) {  // check for "HTTP/1.1 101 response, means Updrage to Websocket OK
        while (client.available()) readLine();
        client.stop();
        return false;
    }
    readLine();
    readLine();
    readLine();
    for (int i = 0; i < 28; i++) {
        key[i] = databuffer[i + 22];  //key contains the Sec-WebSocket-Accept, could be used for verification
    }

    eatHeader();

    sendCode("52"); // Client has to send "52" for the upgrade to websocket

    return true;
}

void SocketIOClient::readLine() {
    for (int i = 0; i < DATA_BUFFER_LEN; i++)
        databuffer[i] = ' ';
    dataptr = databuffer;
    while (client.available() && (dataptr < &databuffer[DATA_BUFFER_LEN - 2])) {
        char c = client.read();
        // Serial.print(c);  //Can be used for debugging
        if (c == 0) {
            // Serial.print("");
        } else if (c == 255) {
            // Serial.println("");
        } else if (c == '\r') {
            ;
        } else if (c == '\n'){
            break;
        }else
            *dataptr++ = c;
    }
    *dataptr = 0;
}

void SocketIOClient::emit(const char *event, const char *content, ackCallback_fn cb) {
	if (cb == NULL) {
		String message = constructMESSAGE(sIOtype_EVENT, event, content);	
		sendMESSAGE(message);
	} else {
		String ackId(_ackId++);
		String message = constructMESSAGE(sIOtype_EVENT, event, content, ackId.c_str());
		_acks[ackId.c_str()] = cb;
		sendMESSAGE(message);
	}
}

void SocketIOClient::send(const char *content) {
    emit("message", content);
}

void SocketIOClient::on(const char *event, callback_fn func) {
    _events[event] = func;
}

void SocketIOClient::sendCode(const String& code) {
    int length = 6 + code.length();
    uint8_t codeBuffer[length];
     /*
	Generating a 32 bits mask requiered for newer version
	*/
    randomSeed(analogRead(0));
    codeBuffer[0] = 0x81; //has to be sent for proper communication
    codeBuffer[1] = code.length() + 128; //size of the message + 128 because message has to be masked

    String mask = "";
    for (int i = 0; i < 4; i++)  //generate a random mask, 4 bytes, ASCII 0 to 9
    {
        char a = random(48, 57);
        codeBuffer[i + 2] = a;
        mask += a;
    }

    for (int i = 0; i < code.length(); i++)
        codeBuffer[i + 6] = code[i] ^ mask[i % 4];  //apply the "mask" to the code

    client.write(codeBuffer, length);
}

void SocketIOClient::sendPing() {
    sendCode("2"); // ping
}

void SocketIOClient::sendPong() {
    sendCode("3"); // pong
}

void SocketIOClient::triggerEvent(const socketIOPacket_t &packet) {
    auto e = _events.find(packet.event.c_str());
    DEBUG_WEBSOCKETS("[socketIO] Trigger event %s\n", packet.event.c_str());
    DEBUG_WEBSOCKETS("[socketIO] Event payload %s\n", packet.data.c_str());
    if (e != _events.end()) {
        ackCallback_fn cb = [this, packet](const char *cb_payload) {
            const String msg = constructMESSAGE(sIOtype_ACK, packet.event.c_str(), cb_payload, packet.id.c_str());
            sendMESSAGE(msg);
        };
        e->second(packet.data, cb);
    }
}

void SocketIOClient::triggerAck(const socketIOPacket_t &packet) {
    auto e = _acks.find(packet.id.c_str());
    if (e != _acks.end()) {
        e->second(packet.event.c_str()); // <- should be data
		_acks.erase(e);
    }
}

String SocketIOClient::constructMESSAGE(socketIOmessageType_t type, const char *event, const char *payload, const char *id) {
    String msg = String((char)eIOtype_MESSAGE) + String((char)type);
    if (id) {
        msg += id;
    }
    msg += "[\"";
    msg += event;
    msg += "\"";
    if (payload) {
        msg += ",";
        if (payload[0] != '{' && payload[0] != '[')
            msg += "\"";
        msg += payload;
        if (payload[0] != '{' && payload[0] != '[')
            msg += "\"";
    }
    msg += "]";
    return msg;
}

void SocketIOClient::sendMESSAGE(const String &message) {
	DEBUG_WEBSOCKETS("[socketIO] send message (%d): %s\n", message.length(), message.c_str());
    int msglength = message.length();
    uint8_t messageBuffer[10 + 4 + msglength];
    messageBuffer[0] = 0x81;  // has to be sent for proper communication
                              // Depending on the size of the message
                                    
    int index;
    if (msglength <= 125) {
        messageBuffer[1] = msglength + 128; //size of the message + 128 because message has to be masked
        index = 2;
    } else if (msglength >= 126 && msglength <= 65535) {
        messageBuffer[1] = 126 + 128;
        messageBuffer[2] = (msglength >> 8) & 255;
        messageBuffer[3] = (msglength)&255;
        index = 4;
    } else {
        messageBuffer[1] = 127 + 128;
        messageBuffer[2] = (msglength >> 56) & 255;
        messageBuffer[3] = (msglength >> 48) & 255;
        messageBuffer[4] = (msglength >> 40) & 255;
        messageBuffer[5] = (msglength >> 32) & 255;
        messageBuffer[6] = (msglength >> 24) & 255;
        messageBuffer[7] = (msglength >> 16) & 255;
        messageBuffer[8] = (msglength >> 8) & 255;
        messageBuffer[9] = (msglength)&255;
        index = 10;
    }

    randomSeed(analogRead(0));
    for (int i = 0; i < 4; i++) {
        messageBuffer[index] = random(48, 57);
        index++;
    }
    for (int i = 0; i < msglength; i++)
        messageBuffer[i + index] = message[i] ^ messageBuffer[(index - 4) + (i % 4)];

    index += msglength;
    client.write(messageBuffer, index);
}

const unsigned int eParseTypeID = 0;
const unsigned int eParseTypeEVENT = 1;
const unsigned int eParseTypeDATA = 2;

socketIOPacket_t SocketIOClient::parse(const std::string &payloadStr) {
    socketIOPacket_t result;
    unsigned int currentParseType = eParseTypeID;
    bool escChar = false;
    bool inString = false;
    bool inJson = false;
    bool inArray = false;
    for (auto c : payloadStr) {
        if (!escChar && c == '"') {
            if (inString)
                inString = false;
            else
                inString = true;
            if (!inJson && !inArray)
                continue;
        }
        if (c == '\\') {
            if (escChar)
                escChar = false;
            else
                escChar = true;
        } else if (escChar)
            escChar = false;

        if (!inJson && !inString && c == '{')
            inJson = true;
        if (inJson && !inString && c == '}')
            inJson = false;

        if (currentParseType > eParseTypeID)
            if (!inArray && !inString && c == '[')
                inArray = true;

        if (!inArray && !inString && !inJson && !escChar) {
            if (c == '[' && currentParseType == eParseTypeID) {
                currentParseType++;
                continue;
            }
            if (c == ',' && currentParseType == eParseTypeEVENT) {
                currentParseType++;
                continue;
            }
            if (c == ']') {
                break;
            }
        }

        if (currentParseType > eParseTypeID)
            if (inArray && !inString && c == ']')
                inArray = false;

        if (currentParseType == eParseTypeID) {
            result.id += c;
        } else if (currentParseType == eParseTypeEVENT) {
            result.event += c;
        } else if (currentParseType == eParseTypeDATA) {
            result.data += c;
        }
    }
    return result;
}
