#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <TinyGPSPlus.h>
#include "secrets.h"

#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GPS_BAUD   9600

#define TOPIC_TELE   "ThanhDat/node01/telemetry"
#define TOPIC_STATUS "ThanhDat/node01/status"

#define SAMPLE_MS        2000   // chu kỳ gửi: 10 phút = 300 bản tin
#define QUEUE_LEN        10
#define SAFETY_MS        500
#define DATA_TIMEOUT_MS  5000
#define SAFE_OUT_PIN     2      // LED onboard đóng vai "bơm"

#define MQTT_SLOW_TEST   0      // đổi thành 1 để thử nghẽn queue
#define MQTT_SLOW_MS     8000

typedef struct {
  uint32_t seq, ts;
  bool valid;
  double lat, lon;
  int sats;      // -1 = null
  int rssi;
} Record;

enum State { ST_DUNG, ST_CHAY, ST_LOI };
const char* STATE_NAME[] = {"DUNG", "CHAY", "LOI"};

QueueHandle_t q;
TinyGPSPlus gps;
HardwareSerial GPSSerial(2);
WiFiClient net;
PubSubClient mqtt(net);

volatile uint32_t g_lastSampleMs = 0, g_beat = 0, g_dropped = 0, g_sent = 0;
volatile bool g_mqttUp = false;
volatile State g_state = ST_DUNG;

void sensorTask(void*) {
  uint32_t seq = 0, next = millis() + SAMPLE_MS;
  for (;;) {
    while (GPSSerial.available()) gps.encode(GPSSerial.read());
    if ((int32_t)(millis() - next) >= 0) {
      next += SAMPLE_MS;
      Record r;
      r.seq = ++seq;
      r.ts = millis();
      r.valid = gps.location.isValid() && gps.location.age() < 3000;
      r.lat = r.valid ? gps.location.lat() : 0;
      r.lon = r.valid ? gps.location.lng() : 0;
      r.sats = gps.satellites.isValid() ? (int)gps.satellites.value() : -1;
      r.rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
      if (xQueueSend(q, &r, 0) != pdTRUE) {      // queue đầy: bỏ bản CŨ NHẤT
        Record old;
        xQueueReceive(q, &old, 0);
        xQueueSend(q, &r, 0);
        g_dropped++;
      }
      g_lastSampleMs = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void buildPayload(const Record& r, char* out, size_t n) {
  char loc[64], sats[8];
  if (r.valid) snprintf(loc, sizeof(loc), "\"lat\":%.6f,\"lon\":%.6f", r.lat, r.lon);
  else         snprintf(loc, sizeof(loc), "\"lat\":null,\"lon\":null");
  if (r.sats >= 0) snprintf(sats, sizeof(sats), "%d", r.sats);
  else             snprintf(sats, sizeof(sats), "null");
  snprintf(out, n,
    "{\"device_id\":\"node01\",\"seq\":%lu,\"ts\":%lu,%s,\"unit\":\"deg\",\"sats\":%s,\"rssi\":%d,\"valid\":%s}",
    (unsigned long)r.seq, (unsigned long)r.ts, loc, sats, r.rssi, r.valid ? "true" : "false");
}

void mqttTask(void*) {
  uint32_t lastTry = 0;
  char buf[200];
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      g_mqttUp = false;
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if (!mqtt.connected()) {
      g_mqttUp = false;
      if (millis() - lastTry > 3000) {
        lastTry = millis();
        Serial.println("[MQTT] dang ket noi...");
                Serial.printf("[MQTT] ESP32 IP=%s -> broker %s:%d\n",
                      WiFi.localIP().toString().c_str(), MQTT_HOST, MQTT_PORT);
        if (mqtt.connect("node01-esp32", MQTT_USER, MQTT_PASS, TOPIC_STATUS, 0, true, "offline")) {
          mqtt.publish(TOPIC_STATUS, "online", true);
          Serial.println("[MQTT] da ket noi");
        } else {
          Serial.printf("[MQTT] that bai rc=%d\n", mqtt.state());
        }
      }
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    g_mqttUp = true;
    mqtt.loop();
    Record r;
    if (xQueueReceive(q, &r, pdMS_TO_TICKS(200)) == pdTRUE) {
      buildPayload(r, buf, sizeof(buf));
      if (mqtt.publish(TOPIC_TELE, buf)) g_sent++;
      Serial.printf("[MQTT] pub seq=%lu\n", (unsigned long)r.seq);
#if MQTT_SLOW_TEST
      vTaskDelay(pdMS_TO_TICKS(MQTT_SLOW_MS));
#endif
    }
  }
}

void safetyTask(void*) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    uint32_t age = millis() - g_lastSampleMs;
    if (g_lastSampleMs == 0)         g_state = ST_DUNG;
    else if (age > DATA_TIMEOUT_MS)  g_state = ST_LOI;
    else                             g_state = ST_CHAY;
    digitalWrite(SAFE_OUT_PIN, g_state == ST_CHAY ? HIGH : LOW);  // lỗi/hết giờ -> về an toàn
    g_beat++;
    if (g_beat % 4 == 0) {   // 2 giây một dòng heartbeat
      Serial.printf("[SAFE] beat=%lu t=%lu state=%s mqtt=%d queue=%u dropped=%lu sent=%lu gpsChars=%lu\n",
        (unsigned long)g_beat, (unsigned long)millis(), STATE_NAME[g_state], (int)g_mqttUp,
        (unsigned)uxQueueMessagesWaiting(q), (unsigned long)g_dropped,
        (unsigned long)g_sent, (unsigned long)gps.charsProcessed());
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(SAFETY_MS));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(SAFE_OUT_PIN, OUTPUT);
  digitalWrite(SAFE_OUT_PIN, LOW);
  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  q = xQueueCreate(QUEUE_LEN, sizeof(Record));
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setSocketTimeout(2);
  xTaskCreatePinnedToCore(sensorTask, "sensor", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(mqttTask,   "mqtt",   6144, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(safetyTask, "safety", 3072, NULL, 3, NULL, 1);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }