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
#include <WebSocketsClient.h>
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

#define TREE_CONNECTION_TIMEOUT 5000
#define TREE_WEBSOCKET_CONNECTION_INTERVAL 5000

Preferences prefs;

WebSocketsServer wss = WebSocketsServer(8080);
WebSocketsClient wsc;

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

void on_loop() {
    Serial.println("Loop detected! Restarting.");
    ESP.restart();
}

String ping_content_type = "cheburchat/ping";

#define KEY_RAND "rand"
uint32_t rand_number;

bool detect_loop_basic(uint8_t* payload, size_t length, size_t& i) {
    if(length == 0) return false;
    if(payload[0] != 0) return false;
    String content_type = "";
    for(; i < length && payload[i] != 0; i++)
        content_type += (char)payload[i];
    if(i >= length) return false;
    if(!content_type.equals(ping_content_type)) return false;
    return true;
}

// a potential DOS vulnerability if we can guess the next rand_number
bool detect_loop(uint8_t* payload, size_t length) {
    size_t i = 1;
    if(!detect_loop_basic(payload, length, i)) return false;
    i++;
    if(i > length - 6) return false;
    IPAddress my_ip = WiFi.softAPIP();
    uint32_t recv_rand = 0;
    for(size_t j = i + 2; j < i + 6; j++) {
        recv_rand <<= 8;
        recv_rand |= payload[j];
    }
    if(my_ip[1] == payload[i] && my_ip[2] == payload[i + 1]
        && recv_rand == rand_number) return true;
    return false;
}

void send_loop_detector() {
    vector<uint8_t> data;
    data.push_back(0);
    for(char c : ping_content_type)
        data.push_back(c);
    data.push_back(0);
    IPAddress my_ip = WiFi.softAPIP();
    data.push_back(my_ip[1]);
    data.push_back(my_ip[2]);
    uint8_t send_rand[4];
    rand_number = rand();
    prefs.putUInt(KEY_RAND, rand_number);
    send_rand[0] = (rand_number >> 24) & 0xff;
    send_rand[1] = (rand_number >> 16) & 0xff;
    send_rand[2] = (rand_number >>  8) & 0xff;
    send_rand[3] = (rand_number >>  0) & 0xff;
    for(uint8_t b : send_rand)
        data.push_back(b);
    wsc.sendBIN(data.data(), data.size());
}

#define KEY_SSID "ssid"
#define KEY_TYPE "type"
#define KEY_PASS "pass"
#define KEY_ADMIN_PASS "admin_pass"
#define KEY_MESSAGE "message"
#define KEY_IP_BYTE1 "byte1"
#define KEY_IP_BYTE2 "byte2"
#define KEY_BSSID "bssid"

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
uint8_t get_byte1() {
    return prefs.getUChar(KEY_IP_BYTE1);
}
uint8_t get_byte2() {
    return prefs.getUChar(KEY_IP_BYTE2);
}
String get_bssid() {
    return prefs.getString(KEY_BSSID, "");
}

vector<uint8_t> clients;

void send_txt(uint8_t num, uint8_t * payload, size_t length, bool ignore_num) {
    if(length == 0) return;
    for(auto client : clients) {
        if(!ignore_num && client == num) continue;
        wss.sendTXT(client, payload);
    }
}

void send_bin(uint8_t num, uint8_t * payload, size_t length, bool ignore_num) {
    if(length == 0) return;
    for(auto client : clients) {
        if(!ignore_num && client == num) continue;
        wss.sendBIN(client, payload, length);
    }
}

void event(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    String message;
    size_t i;
    switch(type) {
        case WStype_CONNECTED:
            clients.push_back(num);
            message = get_message();
            if(message.length() > 0) wss.sendTXT(num, message.c_str());
            Serial.println("Client connected!");
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
            send_txt(num, payload, length, detect_loop_basic(payload, length, i));
            if(wsc.isConnected()) wsc.sendTXT(payload);
            break;
        case WStype_BIN:
            send_bin(num, payload, length, detect_loop_basic(payload, length, i));
            if(wsc.isConnected()) wsc.sendBIN(payload, length);
            break;
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        case WStype_PING:
        case WStype_PONG:
            break;
    }
}

void client_event(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_TEXT:
            if(detect_loop(payload, length)) on_loop();
            send_txt(0, payload, length, true);
            break;
        case WStype_BIN:
            if(detect_loop(payload, length)) on_loop();
            send_bin(0, payload, length, true);
            break;
		case WStype_CONNECTED:
            Serial.println("Connected to another CheburChat server!");
            send_loop_detector();
            break;
        case WStype_ERROR:
        case WStype_DISCONNECTED:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        case WStype_PING:
        case WStype_PONG:
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
    "/library/test/success.html",
    "/success.txt",
    "/canonical.html",
};

void setup() {
    prefs.begin("cheburchat");

    srand(prefs.getUInt(KEY_RAND));
    rand(); // security through obscurity... whatever though

    Serial.begin(115200);
    Serial.println(); Serial.println();
    Serial.println("start");

    Serial.println("admin pass " + get_admin_pass());
    Serial.println("ssid " + get_ssid());
    Serial.println("pass " + get_pass());
    Serial.println("type " + String(get_type(), 10));
    Serial.println("message " + get_message());
    Serial.println("bssid " + get_bssid());
    Serial.println(String("stored ip 10.") + get_byte1() + "." + get_byte2() + ".1");

    WiFi.mode(WIFI_AP_STA);
    uint8_t b1 = get_byte1();
    uint8_t b2 = get_byte2();
    String bssid = get_bssid();
    uint8_t bssid_bin[6];
    bool bssid_enable = true;
    for(size_t i = 0; i < 6; i++) {
        if(i * 3 + 1 >= bssid.length()) {
            bssid_enable = false;
            break;
        }
        bssid_bin[i] = strtoul((String(bssid[i * 3]) + (char)bssid[i * 3 + 1]).c_str(), NULL, 16);
    }

    if(!(b1 == 0 && b2 == 0)) {
        unsigned long start = millis();
        if(get_type())
            bssid_enable
                ? WiFi.begin(get_ssid(), get_pass(), 0, bssid_bin)
                : WiFi.begin(get_ssid(), get_pass());
        else
            bssid_enable
                ? WiFi.begin(get_ssid(), "", 0, bssid_bin)
                : WiFi.begin(get_ssid());
        start = millis();
        while(WiFi.status() != WL_CONNECTED && millis() < start + TREE_CONNECTION_TIMEOUT)
            delay(10);
        if(WiFi.status() == WL_CONNECTED) {
            Serial.println("Connected to WiFi!");

            wsc.begin(WiFi.gatewayIP(), 8080, "/");
            wsc.onEvent(client_event);
            wsc.setReconnectInterval(TREE_WEBSOCKET_CONNECTION_INTERVAL);
        } else Serial.println("Couldn't connect!");
    }

    IPAddress ip_self(10, b1, b2, 1);
    IPAddress ip_mask(255, 255, 255, 0);
    WiFi.softAPConfig(ip_self, ip_self, ip_mask);
    if(get_type())
        WiFi.softAP(get_ssid(), get_pass());
    else
        WiFi.softAP(get_ssid());
    while(WiFi.softAPgetStationNum() == 0) delay(1);
    IPAddress ip = WiFi.softAPIP();
    dnsServer.start(53, "*", ip);

    Serial.println(WiFi.softAPIP().toString());
    Serial.println(WiFi.gatewayIP().toString());
    Serial.println("match = bad");

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
    server.on("/ip", HTTP_POST, []() {
        String admin_pass = server.arg("admin_pass");
        if(admin_pass != get_admin_pass()) {
            server.send(403, "text/plain", "Wrong password!");
            return;
        }
        int byte1 = server.arg("byte1").toInt();
        int byte2 = server.arg("byte2").toInt();
        if(byte1 < 0 || byte1 > 255 || byte2 < 0 || byte2 > 255
            || (byte1 == 0 && byte2 == 0)) {
            server.send(400, "text/plain", "Invalid bytes!");
            return;
        }
        String bssid = server.arg("bssid");
        prefs.putUChar(KEY_IP_BYTE1, byte1);
        prefs.putUChar(KEY_IP_BYTE2, byte2);
        prefs.putString(KEY_BSSID, bssid);
        server.send(200, "text/plain", "OK!");
        ESP.restart();
    });
    server.on("/info", HTTP_GET, []() {
        String mac = WiFi.macAddress();
        server.send(200, "text/plain", "MAC: " + mac);
    });

    server.begin();

    wss.begin();
    wss.onEvent(event);
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
    wss.loop();
    wsc.loop();
}