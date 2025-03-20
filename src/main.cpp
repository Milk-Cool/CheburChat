#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#endif
#include <Preferences.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <WebSocketsServer.h>
#include <vector>

using namespace std;

#include "defaults.h"

DNSServer dnsServer;

#if defined(ESP8266)
ESP8266WebServer server(80);
#elif defined(ESP32)
WebServer server(80);
#endif

WebSocketsServer wss = WebSocketsServer(8080);

String getContentType(String path) {
    if(path.endsWith(".css"))
        return "text/css";
    if(path.endsWith(".js"))
        return "text/javascript";
    if(path.endsWith(".html") || path.indexOf(".") == -1)
        return "text/html";
    return "text/plain";
}

// ChatGPT
bool handleFileRead(String path) {
    if (path.endsWith("/")) path += "index.html";
    
    String contentType = getContentType(path);
    
    if(LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}

vector<uint8_t> clients;

void event(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_CONNECTED:
            clients.push_back(num);
            break;
        case WStype_DISCONNECTED:
            for(auto it = clients.begin(); it != clients.end(); it++) {
                int i = distance(clients.begin(), it);
                if(clients[i] == num) {
                    clients.erase(it);
                    break;
                }
            }
            break;
        case WStype_TEXT:
            if(length == 0) break;
            for(auto client : clients) {
                if(client == num) continue;
                wss.sendTXT(client, payload);
            }
            break;
        case WStype_BIN:
            if(length == 0) break;
            for(auto client : clients) {
                if(client == num) continue;
                wss.sendBIN(client, payload, length);
            }
            break;
        case WStype_ERROR:
            break;
    }
}

String redirect_from[] = {
    "/",
    "/generate_204",
    "/fwlink",
    "/check_network_status.txt",
    "/ncsi.txt",
    "/connecttest.txt",
    "/redirect"
};

void setup() {
    WiFi.mode(WIFI_AP);
    // TODO: put this in preferences
    if(DEFAULT_TYPE)
        WiFi.softAP(DEFAULT_SSID, DEFAULT_PASS);
    else
        WiFi.softAP(DEFAULT_SSID);
    IPAddress ip = WiFi.softAPIP();
    dnsServer.start(53, "*", ip);

    Serial.begin(115200);
    Serial.println(); Serial.println();
    Serial.println("start");

    LittleFS.begin();

    server.serveStatic("/chat/", LittleFS, "/");

    for(String path : redirect_from) {
        server.on(path, HTTP_GET, []() {
            server.sendHeader("Location", "/chat/index.html", true);
            server.send(302, "text/plain", "");
        });
    }
    server.onNotFound([]() {
        if(!handleFileRead(server.uri()))
            server.send(404, "text/plain", (String("Not found (LittleFS): ") + server.uri()).c_str());
    });

    server.begin();

    wss.begin();
    wss.onEvent(event);
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    wss.loop();
}