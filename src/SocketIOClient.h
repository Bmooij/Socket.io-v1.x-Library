/*
socket.io-arduino-client: a Socket.IO client for the Arduino
Based on the Kevin Rohling WebSocketClient & Bill Roy Socket.io Lbrary
Copyright 2015 Florent Vidal
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:
JSON support added using https://github.com/bblanchon/ArduinoJson
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
#ifndef _SOCKET_IO_CLIENT_H
#define _SOCKET_IO_CLIENT_H

#include <Arduino.h>
#include <map>

#if defined(W5100)
#include <Ethernet.h>
#include "SPI.h"					//For W5100
#elif defined(ENC28J60)
#include <UIPEthernet.h>
#include "SPI.h"					//For ENC28J60
#elif defined(ESP8266)
#include <ESP8266WiFi.h>				//For ESP8266
#elif defined(ESP32)
#include <WiFi.h>					//For ESP32
#elif (!defined(ESP32) && !defined(ESP8266) && !defined(W5100) && !defined(ENC28J60))	//If no interface is defined
#error "Please specify an interface such as W5100, ENC28J60, ESP8266 or ESP32"
#error "above your includes like so : #define ESP8266 "
#endif

#ifndef NODEBUG_WEBSOCKETS
#define DEBUG_WEBSOCKETS(...) Serial.printf( __VA_ARGS__ )
#endif


#ifndef DEBUG_WEBSOCKETS
#define DEBUG_WEBSOCKETS(...)
#define NODEBUG_WEBSOCKETS
#endif

// Length of static data buffers
#define DATA_BUFFER_LEN 120
#define SID_LEN 24

struct socketIOPacket_t {
    String id = "";
    String event = "";
    String data = "";
};

typedef enum : char {
    eIOtype_OPEN = '0', ///< Sent from the server when a new transport is opened (recheck)
    eIOtype_CLOSE = '1', ///< Request the close of this transport but does not shutdown the connection itself.
    eIOtype_PING = '2', ///< Sent by the client. Server should answer with a pong packet containing the same data
    eIOtype_PONG = '3', ///< Sent by the server to respond to ping packets.
    eIOtype_MESSAGE = '4', ///< actual message, client and server should call their callbacks with the data
    eIOtype_UPGRADE = '5', ///< Before engine.io switches a transport, it tests, if server and client can communicate over this transport. If this test succeed, the client sends an upgrade packets which requests the server to flush its cache on the old transport and switch to the new transport.
    eIOtype_NOOP = '6', ///< A noop packet. Used primarily to force a poll cycle when an incoming websocket connection is received.
} engineIOmessageType_t;

typedef enum : char {
    sIOtype_CONNECT = '0',
    sIOtype_DISCONNECT = '1',
    sIOtype_EVENT = '2',
    sIOtype_ACK = '3',
    sIOtype_ERROR = '4',
    sIOtype_BINARY_EVENT = '5',
    sIOtype_BINARY_ACK = '6',
} socketIOmessageType_t;

typedef std::function<void (const char * payload)> ackCallback_fn;
typedef std::function<void (const String &payload, ackCallback_fn)> callback_fn;

class SocketIOClient {
public:
	bool connect(char hostname[], int port = 80);
	bool connectHTTP(char hostname[], int port = 80);
	bool connected();
	void disconnect();
	bool reconnect(char hostname[], int port = 80);
	bool monitor();
	void emit(const char *event, const char *content, ackCallback_fn = NULL);
	void send(const char *content);
	void on(const char* event, callback_fn);
	void heartbeat(int select);
	void clear();
private:
	void parser(int index);
	void sendHandshake(char hostname[]);
#if defined(W5100) || defined(ENC28J60)
	EthernetClient client;				//For ENC28J60 or W5100
#endif
#if defined(ESP8266) || defined(ESP32)
	WiFiClient client;				//For ESP8266 or ESP32
#endif

	bool readHandshake();
	void readLine();
	char *dataptr;
	char databuffer[DATA_BUFFER_LEN];
	char sid[SID_LEN];
	char key[28];
	char *hostname;
	char *nsp;
	int port;

	void findColon(char which);
	void terminateCommand(void);
	bool waitForInput(void);
	void eatHeader(void);

	void sendMESSAGE(const String &message);
	String constructMESSAGE(socketIOmessageType_t type, const char* event, const char* payload = NULL, const char * id = NULL);
	void triggerEvent(const socketIOPacket_t &packet);
	void triggerAck(const socketIOPacket_t &packet);
	std::map<String, callback_fn> _events;
	std::map<String, ackCallback_fn> _acks;
	size_t _ackId = 1;

	/**
	 * Parses the payload into a socketIOPacket_t.
	 * The payload has the following format: ID[EVENT,DATA]
	 * socketIOPacket_t contains the id, event and data.
	 * @param payload std::string
	 */
	socketIOPacket_t parse(const std::string &payload);
};

#endif
