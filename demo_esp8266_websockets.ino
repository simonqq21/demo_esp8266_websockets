#include <ArduinoJson.h>
#include <ESP8266WiFi.h> 
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// LED and button pins 
#define TYPELED "led"
#define TYPEBTN "button" 
#define LEDCOUNT 3
#define BTNCOUNT 2
int LEDS[] = {2, 3, 4};
int BUTTONS[] = {5, 14}; 
char type[16];
int globalLEDIndex = 0; 
int globalBtnIndex = 0;  

// async web server
AsyncWebServer server(80); 
AsyncWebSocket ws("/ws"); 
StaticJsonDocument<70> inputDoc;
StaticJsonDocument<70> outputDoc;
char strData[70];

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
int onIntervals[] = {1000, 1000, 1000}, offIntervals[] = {1000, 1000, 1000};
uint32_t lastTimeUpdated = 0;
bool buttonStatus[] = {false, false};
bool presButtonBits[] = {false, false};
bool prevButtonBits[] = {false, false};
unsigned long lastDebounceTimes[] = {0, 0};
const int debounceDelay = 10;

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

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp("new", (char*)data) == 0) {
      for (int i=0;i<LEDCOUNT;i++) {
        globalLEDIndex = i;
        sendLEDJSON();
      }
      for (int i=0;i<BTNCOUNT;i++) {
        globalBtnIndex = i; 
        sendBtnJSON();
      }
    }
    else {
      DeserializationError error = deserializeJson(inputDoc, (char*)data); 
      if (error) {
        Serial.print("deserializeJson failed: ");
        Serial.println(error.f_str());
      }
      else 
        Serial.println("deserializeJson success");
      strcpy(type, inputDoc["type"]);
      if (strcmp(type, TYPELED) == 0) {   
        controlLED();
      }
    }
  }
}

void sendAll() {

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

void controlLED() {
  globalLEDIndex = (int) inputDoc["index"];
  int state = (int) inputDoc["state"];
  switch (state) {
    case 0:
      ledBlinks[globalLEDIndex] = false;
      ledBits[globalLEDIndex] = false;
      break;
    case 1:
      ledBlinks[globalLEDIndex] = false;
      ledBits[globalLEDIndex] = true;
      break;
    case 2:
      ledBits[globalLEDIndex] = false;
      ledBlinks[globalLEDIndex] = true;
  }
  digitalWrite(LEDS[globalLEDIndex], ledBits[globalLEDIndex]);
  sendLEDJSON();
}

void sendLEDJSON() {
  outputDoc.clear();
  outputDoc["type"] = TYPELED;
  outputDoc["index"] = globalLEDIndex;
  outputDoc["state"] = ledBits[globalLEDIndex];
  outputDoc["blink"] = ledBlinks[globalLEDIndex];
  serializeJson(outputDoc, strData);
  ws.textAll(strData);
}

void sendBtnJSON() {
  outputDoc.clear(); 
  outputDoc["type"] = TYPEBTN;
  outputDoc["index"] = globalBtnIndex; 
  outputDoc["state"] = presButtonBits[globalBtnIndex];
  serializeJson(outputDoc, strData);
  ws.textAll(strData);
}

// String processor(const String& var) {
//   Serial.println(var);
//   if (var == "LED0STATE") {
//     return getLEDStringState(ledBits[0], ledBlinks[0]);
//   }
//   else if (var == "LED1STATE") {
//     return getLEDStringState(ledBits[1], ledBlinks[1]);
//   }
//   else if (var == "LED2STATE") {
//     return getLEDStringState(ledBits[2], ledBlinks[2]);
//   }
//   else if (var == "BTN1STATE") {
//     return getBtnStringState(presButtonBits[0]);
//   }
//   return "X";
// }

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

void blinkLED(int index) {
  if (ledBlinks[index]) {
    if (millis() - lastTimeBlinked[index] > offIntervals[index] && !ledBits[index]) {
      lastTimeBlinked[index] = millis();
      ledBits[index] = true;
      digitalWrite(LEDS[index], ledBits[index]);
    }
    if (millis() - lastTimeBlinked[index] > onIntervals[index] && ledBits[index]) {
      lastTimeBlinked[index] = millis();
      ledBits[index] = false;
      digitalWrite(LEDS[index], ledBits[index]);
    }
  } 
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
    request->send(LittleFS, "/index.html", String(), false);});
  server.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  ws.cleanupClients(); 
    // blink the LEDs that are supposed to blink 
  for (int i=0;i<LEDCOUNT;i++)
    blinkLED(i);
  for (int i=0;i<BTNCOUNT;i++) {
    buttonStatus[i] = digitalRead(BUTTONS[i]); 
  }
  for (int i=0;i<BTNCOUNT;i++) {
    if (buttonStatus[i] != prevButtonBits[i]) {
      lastDebounceTimes[i] = millis(); 
    }
    if (millis() - lastDebounceTimes[i] > debounceDelay) {
      lastDebounceTimes[i] = millis();
      if (presButtonBits[i] != buttonStatus[i]) {
        presButtonBits[i] = buttonStatus[i];
        globalBtnIndex = i;
        sendBtnJSON();
      }
    }
  }
  for (int i=0;i<BTNCOUNT;i++) {
    prevButtonBits[i] = buttonStatus[i]; 
  }
}
