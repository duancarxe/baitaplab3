
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <PubSubClient.h>
#include <math.h>

// Cau hinh chan
#define LED_PIN 25
#define BUZZER_PIN 26
#define SDA_PIN 21
#define SCL_PIN 22

// WiFi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

const char* topic_data = "runner/telemetry";
const char* topic_led = "runner/control/led";
const char* topic_buzzer = "runner/control/buzzer";

// Nguong canh bao
#define SEA_LEVEL_PRESSURE 101325.0
#define ALT_CHANGE 100.0
#define TEMP_MAX 35.0
#define TEMP_MIN 10.0

Adafruit_BMP085 bmp;
WiFiClient espClient;
PubSubClient mqtt(espClient);

struct SensorData {
  float temperature;
  float pressure;
  float altitude;
  float initialAltitude;
  float altitudeChange;
  bool altitudeAlert;
  bool temperatureAlert;
};

struct AlertData {
  float temperature;
  float altitude;
  float initialAltitude;
  float altitudeChange;
  bool altitudeAlert;
  bool temperatureAlert;
};

QueueHandle_t dataQueue;
QueueHandle_t dataAlert;
SemaphoreHandle_t serialMutex;

portMUX_TYPE controlMux = portMUX_INITIALIZER_UNLOCKED;

volatile bool manualLed = false;
volatile bool manualBuzzer = false;

// Bao ve Serial
void lockSerial() {
  xSemaphoreTake(serialMutex, portMAX_DELAY);
}

void unlockSerial() {
  xSemaphoreGive(serialMutex);
}

// Nhan lenh dieu khien tu MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.trim();
  message.toUpperCase();

  bool state;

  if (message == "ON" || message == "1") {
    state = true;
  } else if (message == "OFF" || message == "0") {
    state = false;
  } else {
    return;
  }

  portENTER_CRITICAL(&controlMux);

  if (strcmp(topic, topic_led) == 0) {
    manualLed = state;
  }

  if (strcmp(topic, topic_buzzer) == 0) {
    manualBuzzer = state;
  }

  portEXIT_CRITICAL(&controlMux);

  lockSerial();
  Serial.print("MQTT Control: ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(message);
  unlockSerial();
}

// Ket noi lai MQTT
void reconnectMQTT() {
  while (!mqtt.connected()) {
    lockSerial();
    Serial.println("Dang ket noi MQTT...");
    unlockSerial();

    String clientId = "ESP32Runner-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqtt.connect(clientId.c_str())) {
      lockSerial();
      Serial.println("MQTT connected!");
      unlockSerial();

      mqtt.subscribe(topic_led);
      mqtt.subscribe(topic_buzzer);
    } else {
      lockSerial();
      Serial.print("MQTT failed, state = ");
      Serial.println(mqtt.state());
      unlockSerial();

      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
}

// Task doc cam bien - Core 1
void TaskReadData(void* pvParameters) {
  SensorData sensor;
  AlertData alert;

  bool firstReading = true;
  float initialAltitude = 0.0;

  while (true) {
    sensor.temperature = bmp.readTemperature();
    sensor.pressure = bmp.readPressure();
    sensor.altitude = bmp.readAltitude(SEA_LEVEL_PRESSURE);

    if (!isfinite(sensor.temperature) ||
        !isfinite(sensor.pressure) ||
        !isfinite(sensor.altitude)) {

      lockSerial();
      Serial.println("BMP180 DATA ERROR!");
      unlockSerial();

      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (firstReading) {
      initialAltitude = sensor.altitude;
      firstReading = false;

      lockSerial();
      Serial.print("Initial altitude: ");
      Serial.println(initialAltitude);
      unlockSerial();
    }

    sensor.initialAltitude = initialAltitude;

    sensor.altitudeChange =
      fabsf(sensor.altitude - initialAltitude);

    sensor.altitudeAlert =
      sensor.altitudeChange >= ALT_CHANGE;

    sensor.temperatureAlert =
      (sensor.temperature > TEMP_MAX ||
       sensor.temperature < TEMP_MIN);

    lockSerial();

    Serial.println("\n===== SENSOR DATA =====");

    Serial.print("Temperature: ");
    Serial.print(sensor.temperature);
    Serial.println(" C");

    Serial.print("Pressure: ");
    Serial.print(sensor.pressure / 100.0);
    Serial.println(" hPa");

    Serial.print("Altitude: ");
    Serial.print(sensor.altitude);
    Serial.println(" m");

    Serial.print("Altitude change: ");
    Serial.print(sensor.altitudeChange);
    Serial.println(" m");

    Serial.print("Altitude alert: ");
    Serial.println(sensor.altitudeAlert ? "ON" : "OFF");

    Serial.print("Temperature alert: ");
    Serial.println(sensor.temperatureAlert ? "ON" : "OFF");

    unlockSerial();

    xQueueSend(dataQueue, &sensor, 0);

    alert.temperature = sensor.temperature;
    alert.altitude = sensor.altitude;
    alert.initialAltitude = sensor.initialAltitude;
    alert.altitudeChange = sensor.altitudeChange;
    alert.altitudeAlert = sensor.altitudeAlert;
    alert.temperatureAlert = sensor.temperatureAlert;

    xQueueOverwrite(dataAlert, &alert);

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// Task gui du lieu MQTT - Core 0
void TaskSendData(void* pvParameters) {
  SensorData sensor;

  WiFi.begin(ssid, password);

  lockSerial();
  Serial.println("Connecting to WiFi...");
  unlockSerial();

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  lockSerial();
  Serial.println("WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  unlockSerial();

  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);

  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (!mqtt.connected()) {
      reconnectMQTT();
    }

    mqtt.loop();

    if (xQueueReceive(dataQueue, &sensor,
                      pdMS_TO_TICKS(100)) == pdTRUE) {

      char json[512];

      snprintf(json, sizeof(json),
        "{"
        "\"temperature\":%.2f,"
        "\"pressure\":%.2f,"
        "\"altitude\":%.2f,"
        "\"initialAltitude\":%.2f,"
        "\"altitudeChange\":%.2f,"
        "\"altitudeAlert\":%s,"
        "\"temperatureAlert\":%s,"
        "\"altitudeThreshold\":%.2f,"
        "\"tempMax\":%.2f,"
        "\"tempMin\":%.2f"
        "}",

        sensor.temperature,
        sensor.pressure / 100.0,
        sensor.altitude,
        sensor.initialAltitude,
        sensor.altitudeChange,
        sensor.altitudeAlert ? "true" : "false",
        sensor.temperatureAlert ? "true" : "false",
        (double)ALT_CHANGE,
        (double)TEMP_MAX,
        (double)TEMP_MIN
      );

      bool result = mqtt.publish(topic_data, json);

      lockSerial();

      if (result) {
        Serial.println("MQTT PUBLISH SUCCESS!");
        Serial.println(json);
      } else {
        Serial.println("MQTT PUBLISH FAILED!");
      }

      unlockSerial();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Task canh bao - Core 1
void TaskAlert(void* pvParameters) {
  AlertData alert = {};

  bool gotData = false;
  bool lastAutoAlert = false;

  while (true) {
    if (xQueueReceive(dataAlert, &alert,
                      pdMS_TO_TICKS(100)) == pdTRUE) {
      gotData = true;

      lockSerial();

      Serial.println("\n===== ALERT SYSTEM =====");
      Serial.print("Temperature alert: ");
      Serial.println(alert.temperatureAlert ? "YES" : "NO");

      Serial.print("Altitude alert: ");
      Serial.println(alert.altitudeAlert ? "YES" : "NO");

      unlockSerial();
    }

    bool ledManual;
    bool buzzerManual;

    portENTER_CRITICAL(&controlMux);
    ledManual = manualLed;
    buzzerManual = manualBuzzer;
    portEXIT_CRITICAL(&controlMux);

    bool autoAlert = gotData &&
      (alert.altitudeAlert || alert.temperatureAlert);

    bool ledState = autoAlert || ledManual;
    bool buzzerState = autoAlert || buzzerManual;

    digitalWrite(LED_PIN, ledState ? HIGH : LOW);

    if (buzzerState) {
      tone(BUZZER_PIN, 1000);
    } else {
      noTone(BUZZER_PIN);
    }

    if (autoAlert != lastAutoAlert) {
      lockSerial();

      Serial.println(autoAlert
        ? "AUTOMATIC WARNING ON!"
        : "Automatic warning OFF");

      unlockSerial();

      lastAutoAlert = autoAlert;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// Khoi tao he thong
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!bmp.begin()) {
    Serial.println("BMP180 NOT FOUND!");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("BMP180 initialized!");

  serialMutex = xSemaphoreCreateMutex();

  dataQueue = xQueueCreate(10, sizeof(SensorData));
  dataAlert = xQueueCreate(1, sizeof(AlertData));

  if (serialMutex == NULL ||
      dataQueue == NULL ||
      dataAlert == NULL) {

    Serial.println("FreeRTOS resource creation failed!");

    while (true) {
      delay(1000);
    }
  }

  xTaskCreatePinnedToCore(
    TaskReadData,
    "TaskReadData",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    TaskSendData,
    "TaskSendData",
    6144,
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    TaskAlert,
    "TaskAlert",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  Serial.println("Runner Monitoring System Started!");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
