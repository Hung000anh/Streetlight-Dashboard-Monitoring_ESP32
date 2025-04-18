#include <WiFi.h>
#include <PubSubClient.h>

#define LDR_PIN 34
#define RELAY_PIN 23

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqttServer = "thingsboard.cloud";
const int mqttPort = 1883;
const char* token = "jSNXYT0BdFgS69jXXfJr";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastSend = 0;
unsigned long startTime = 0;
const int interval = 5000;

String currentMode = "manual";
String lightStatus = "OFF";
int on_count = 0;
bool prevRelay = false;

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to ThingsBoard...");
    if (client.connect("ESP32", token, NULL)) {
      Serial.println(" Connected!");
      client.subscribe("v1/devices/me/rpc/request/+");

      // Gửi thông tin ban đầu
      String initAttr = "{\"model\":\"ESP32-Relay-LDR\",\"firmware_version\":\"1.0\"}";
      client.publish("v1/devices/me/attributes", initAttr.c_str());
    } else {
      Serial.print(" Failed. Error: ");
      Serial.print(client.state());
      delay(1000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String request = "";
  for (int i = 0; i < length; i++) {
    request += (char)payload[i];
  }
  Serial.print("RPC Received: ");
  Serial.println(request);

  if (request.indexOf("setStatus") != -1) {
    if (request.indexOf("ON") != -1) {
      lightStatus = "ON";
    } else if (request.indexOf("OFF") != -1) {
      lightStatus = "OFF";
    }
  }

  if (request.indexOf("setMode") != -1) {
    if (request.indexOf("auto") != -1) {
      currentMode = "auto";
    } else if (request.indexOf("manual") != -1) {
      currentMode = "manual";
    }
  }

  // Phản hồi trạng thái
  // Gửi riêng từng attribute
  String modePayload = "{\"mode\":\"" + currentMode + "\"}";
  client.publish("v1/devices/me/attributes", modePayload.c_str());

  String statusPayload = "{\"status\":\"" + lightStatus + "\"}";
  client.publish("v1/devices/me/attributes", statusPayload.c_str());
}

void sendTelemetry(int ldr, String status) {
  unsigned long uptime = (millis() - startTime) / 1000;
  String payload = "{";
  payload += "\"ambient_light\":" + String(ldr) + ",";
  payload += "\"status\":\"" + status + "\",";
  payload += "\"mode\":\"" + currentMode + "\",";
  payload += "\"uptime\":" + String(uptime) + ",";
  payload += "\"on_count\":" + String(on_count) + ",";
  payload += "\"model\":\"LDR/Relay\"";
  payload += "}";

  client.publish("v1/devices/me/telemetry", payload.c_str());
  Serial.println("Telemetry Sent: " + payload);
}

void setup() {
  Serial.begin(115200);
  pinMode(LDR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  connectToWiFi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  connectToMQTT();
  startTime = millis();
  Serial.println("System Ready");
}

void loop() {
  if (!client.connected()) connectToMQTT();
  client.loop();

  if (millis() - lastSend > interval) {
    lastSend = millis();
    int rawLdrValue = analogRead(LDR_PIN);
    rawLdrValue = constrain(rawLdrValue, 0, 1023);
    int ldrValue = map(rawLdrValue, 0, 1023, 1023, 0);
    String status;

    if (currentMode == "auto") {
      if (ldrValue > 500) {
        digitalWrite(RELAY_PIN, HIGH);
        status = "ON";
      } else {
        digitalWrite(RELAY_PIN, LOW);
        status = "OFF";
      }
    } else {
      status = lightStatus;
      digitalWrite(RELAY_PIN, (status == "ON") ? HIGH : LOW);
    }

    // Đếm số lần bật đèn
    bool currentRelay = (status == "ON");
    if (currentRelay && !prevRelay) on_count++;
    prevRelay = currentRelay;

    sendTelemetry(ldrValue, status);
  }
}
