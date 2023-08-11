#include <ESP8266WiFi.h> 
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// LED and button pins 
int LEDS[3] = {2, 3, 4};
int BUTTONS[1] = {5}; 
int globalLEDIndex = 0; 
int globalBtnIndex = 0;  

// async web server
AsyncWebServer server(80); 
AsyncWebSocket ws("/ws"); 
char strData[256];

//static IP address configuration 
IPAddress local_IP(192,168,5,75);
IPAddress gateway(192,168,5,1);
IPAddress subnet(255,255,255,0);
//IPAddress primaryDNS(8,8,8,8);
//IPAddress secondaryDNS(8,8,4,4);
#define APMODE false

// status variables 
bool ledBits[] = {false, false, false};
bool ledBlinks[] = {false, false, false};
unsigned long lastTimeBlinked[] = {0, 0, 0};
int onIntervals[] = {1000, 1000, 1000}, offIntervals[] = {1000, 3000, 7000};
uint32_t lastTimeUpdated = 0;
bool buttonBits[] = {false};

// current time 
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000; 

#define LOCAL_SSID "QUE-STARLINK"
#define LOCAL_PASS "Quefamily01259"

File indexPage;  

#define ONTEXT "ON"
#define OFFTEXT "OFF"
#define BLINKTEXT "BLINKING"
#define HIGHTEXT "HIGH"
#define LOWTEXT "LOW"

void printWiFi() {
  Serial.println(" ");
  Serial.println("WiFi connected.");
  Serial.print("WiFi SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address: "); 
  Serial.println(WiFi.localIP()); 
  long rssi = WiFi.RSSI(); 
  Serial.print("Signal strength (RSSI): "); 
  Serial.print(rssi);
  Serial.println(" dBm");
}

void notifyClients() {
  ws.textAll(String(strData));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    strcpy(strData,(char*)data);
    Serial.println(strData);
    // if (strcmp((char*)data, "toggle") == 0) {
    //   ledState = !ledState;
    notifyClients();
    // }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    switch (type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var) {
  Serial.println(var);
  if (var == "LED1STATE") {
    return getLEDStringState(ledBits[0], ledBlinks[0]);
  }
  else if (var == "LED2STATE") {
    return getLEDStringState(ledBits[1], ledBlinks[1]);
  }
  else if (var == "LED3STATE") {
    return getLEDStringState(ledBits[2], ledBlinks[2]);
  }
  else if (var == "BTN1STATE") {
    return getBtnStringState(buttonBits[0]);
  }
  else if (var == "LED1STATE") {
    return getLEDStringState(ledBits[0], ledBlinks[0]);
  }
  return "X";
}

String getLEDStringState(bool bit, bool blink) {
  if (blink)
    return BLINKTEXT;
  else {
    if (bit)
      return ONTEXT;
    else return OFFTEXT;
  }
}

String getBtnStringState(bool bit) {
  if (bit)
    return HIGHTEXT;
  else return LOWTEXT;
}

void setup() {
  Serial.begin(115200); 
  // littleFS 
  if (!LittleFS.begin()) {
    Serial.println("An error occured while mounting LittleFS.");
  }
  // pins 
  for (int pin: LEDS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  for (int pin: BUTTONS) {
    pinMode(pin, INPUT_PULLUP);
  }
  // wifi 
  if (APMODE) {
    Serial.println("Starting AP");
    if (!WiFi.softAP("simon")) {
      Serial.println("Soft AP failed to configure.");
    }
    WiFi.softAPConfig(local_IP, gateway, subnet);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  }
  else {
    Serial.print("Connecting to "); 
    Serial.println(LOCAL_SSID);
  
    if (!WiFi.config(local_IP, gateway, subnet)) {
      Serial.println("Station failed to configure.");
    }
    
    WiFi.begin(LOCAL_SSID, LOCAL_PASS); 
    while (WiFi.status() != WL_CONNECTED) {
      delay(500); 
      Serial.print(".");
    }
    //  print local IP address and start web server 
    printWiFi();
  }
  // initialize websocket 
  initWebSocket(); 

  // route for root web page 
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", String(), false, processor);});
  server.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  ws.cleanupClients(); 
//  if (millis() - previousTime >= timeoutTime) {
//    previousTime = millis(); s
//    ws.textAll("button 0");
//  }
}
