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

#include "defaults.h"

DNSServer dnsServer;

#if defined(ESP8266)
ESP8266WebServer server(80);
#elif defined(ESP32)
WebServer server(80);
#endif

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

    server.on("/", HTTP_GET, []() {
        server.sendHeader("Location", "/chat/index.html", true);
        server.send(302, "text/plain", "");
    });
    server.on("/generate_204", HTTP_GET, []() {
        server.sendHeader("Location", "/chat/index.html", true);
        server.send(302, "text/plain", "");
    });
    server.on("/fwlink", HTTP_GET, []() {
        server.sendHeader("Location", "/chat/index.html", true);
        server.send(302, "text/plain", "");
    });
    server.onNotFound([]() {
        if(!handleFileRead(server.uri()))
            server.send(404, "text/plain", "Not found (LittleFS)");
    });

    server.begin();
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
}