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

Preferences prefs;

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

#define KEY_SSID "ssid"
#define KEY_TYPE "type"
#define KEY_PASS "pass"
#define KEY_ADMIN_PASS "admin_pass"
#define KEY_MESSAGE "message"

String get_ssid() {
    return prefs.getString(KEY_SSID, DEFAULT_SSID);
}
int get_type() {
    return prefs.getInt(KEY_TYPE, DEFAULT_TYPE);
}
String get_pass() {
    return prefs.getString(KEY_PASS, DEFAULT_PASS);
}
String get_admin_pass() {
    return prefs.getString(KEY_ADMIN_PASS, DEFAULT_ADMIN_PASS);
}
String get_message() {
    return prefs.getString(KEY_MESSAGE, DEFAULT_MESSAGE);
}

vector<uint8_t> clients;

void event(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    String message;
    switch(type) {
        case WStype_CONNECTED:
            clients.push_back(num);
            message = get_message();
            if(message.length() > 0) wss.sendTXT(num, message.c_str());
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
    "/redirect",
    "/hotspot-detect.html",
};

void setup() {
    prefs.begin("cheburchat");

    Serial.begin(115200);
    Serial.println(); Serial.println();
    Serial.println("start");

    Serial.println("admin pass " + get_admin_pass());
    Serial.println("ssid " + get_ssid());
    Serial.println("pass " + get_pass());
    Serial.println("type " + String(get_type(), 10));
    Serial.println("message " + get_message());

    WiFi.mode(WIFI_AP);
    if(get_type())
        WiFi.softAP(get_ssid(), get_pass());
    else
        WiFi.softAP(get_ssid());
    while(WiFi.softAPgetStationNum() == 0) delay(1);
    IPAddress ip = WiFi.softAPIP();
    dnsServer.start(53, "*", ip);

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
    server.on("/settings", HTTP_POST, []() {
        String admin_pass = server.arg("admin_pass");
        if(admin_pass != get_admin_pass()) {
            server.send(403, "text/plain", "Wrong password!");
            return;
        }
        int type = server.arg("type") == "on" ? 1 : 0;
        String pass = server.arg("pass");
        if(type && (pass.length() < 8 || pass.length() > 63)) {
            server.send(400, "text/plain", "Invalid password length!");
            return;
        }
        String ssid = server.arg("ssid");
        if(ssid.length() == 0 || ssid.length() > 63) {
            server.send(400, "text/plain", "Invalid SSID length!");
            return;
        }
        String message = server.arg("message");
        prefs.putString(KEY_SSID, ssid);
        prefs.putString(KEY_PASS, pass);
        prefs.putInt(KEY_TYPE, type);
        prefs.putString(KEY_MESSAGE, message);
        server.send(200, "text/plain", "OK!");
        ESP.restart();
    });
    server.on("/password", HTTP_POST, []() {
        String admin_pass = server.arg("admin_pass");
        if(admin_pass != get_admin_pass()) {
            server.send(403, "text/plain", "Wrong password!");
            return;
        }
        String new_pass = server.arg("new_pass");
        String new_pass_repeat = server.arg("new_pass_repeat");
        if(new_pass != new_pass_repeat) {
            server.send(400, "text/plain", "Passwords don't match!");
            return;
        }
        prefs.putString(KEY_ADMIN_PASS, new_pass);
        server.send(200, "text/plain", "OK!");
        ESP.restart();
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