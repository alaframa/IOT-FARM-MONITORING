#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

#define DHTPIN 27 
#define DHTTYPE DHT22

#define MOISTURE_SOIL_PIN 34 

#define LDR_PIN 35 
const float GAMMA = 0.7;
const float RL10 = 50;

#define ANEMO_PIN 12 
volatile int countertickwind = 0;
float settingwindspeed = 0.0875; 

#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET 25200
#define UTC_OFFSET_DST 0


struct SensorReadings {
    float soil_moisture;
    float air_temperature;
    float wind_speed;
    int sunlight_intensity;
};

struct SensorData {
    String sensor_code;
    String location;
    String timestamp;
    SensorReadings data;
};

SensorData sensordata;
bool state = false;
unsigned long lastUpdateTime0 = 0;
unsigned long lastUpdateTime1 = 0;
int interval0 = 5000;
int interval1 = 5000;


void core1Task(void *parameter) {
  while (1) {
     if (state && millis() - lastUpdateTime1 >= interval1) {
        sendSensorData();        
        resetValues();
        lastUpdateTime1 = millis(); 
     }
    vTaskDelay(1000 );
  }
}

void core0Task(void *parameter) {
    while (1) {
        if (millis() - lastUpdateTime0 >= interval0) {
            sensordata.timestamp = printLocalTime();
            sensordata.sensor_code = "FF-001";
            sensordata.location = "Blok Timur A";
            sensordata.data.soil_moisture = readSoilMoisture();
            sensordata.data.air_temperature = readAirTemperature();
            sensordata.data.wind_speed = counterwindspeed(countertickwind);
            sensordata.data.sunlight_intensity = readSunlightIntensity();


            Serial.println(sensordata.timestamp);
            Serial.println(sensordata.sensor_code);
            Serial.println(sensordata.location);
            Serial.print(sensordata.data.soil_moisture);      Serial.println("  soil");
            Serial.print(sensordata.data.air_temperature);    Serial.println("  air_t");
            Serial.print(sensordata.data.wind_speed);         Serial.println("  wind");
            Serial.print(sensordata.data.sunlight_intensity); Serial.println("  light");
            state = true;
            lastUpdateTime0 = millis(); 
        }
        vTaskDelay(1000);
    }
}

void IRAM_ATTR anemoISR() {
    countertickwind++;
}

void setup() {
    Serial.begin(9600);
    xTaskCreatePinnedToCore(core1Task, "Core1Task", 4000, NULL, 1, NULL, 1); // Reduce stack size to 4000 bytes
    xTaskCreatePinnedToCore(core0Task, "Core0Task", 4000, NULL, 1, NULL, 0); // Reduce stack size to 4000 bytes

    WiFi.begin("Wokwi-GUEST", "");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");



    pinMode(ANEMO_PIN, INPUT);
    pinMode(DHTPIN, INPUT);
    pinMode( LDR_PIN,INPUT);
    pinMode( MOISTURE_SOIL_PIN,INPUT);
    

    attachInterrupt(digitalPinToInterrupt(ANEMO_PIN), anemoISR, FALLING);

    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
    printLocalTime();
}

void loop() {
    delay(1);
}

float counterwindspeed(int tick) {
   return settingwindspeed * tick *interval1/1000;
}

float readSoilMoisture() {
    return 100.0 - ((analogRead(MOISTURE_SOIL_PIN) / 4095.0) * 100.0);
}

float readAirTemperature() {
    DHT dht(DHTPIN, DHTTYPE);
    return dht.readTemperature(); 
}

int readSunlightIntensity() {
  int analogValue = analogRead(A0);
  float voltage = analogValue / 4096. * 5;
  float resistance = 2000 * voltage / (1 - voltage / 5);
  float lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));
  return lux ;
}


String printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return "";
    }

    char timeStr[20];
    sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec);
    return String(timeStr);
}

void sendSensorData() {
    HTTPClient http;

    http.begin("https://cisea.bukitasam.co.id/api-iot/api/v1/iot/iot-test/post");
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"sensor_code\":\"" + sensordata.sensor_code + "\",";
    jsonData += "\"location\":\"" + sensordata.location + "\",";
    jsonData += "\"timestamp\":\"" + sensordata.timestamp + "\",";
    jsonData += "\"data\":{\"soil_moisture\":" + String(sensordata.data.soil_moisture, 2) + ",";
    jsonData += "\"air_temperature\":" + String(sensordata.data.air_temperature, 2) + ",";
    jsonData += "\"wind_speed\":" + String(sensordata.data.wind_speed, 2) + ",";
    jsonData += "\"sunlight_intensity\":" + String(sensordata.data.sunlight_intensity) + "}}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("HTTP Response code: " + String(httpResponseCode));
        Serial.println(response);
    } else {
        Serial.println("Error sending data");
    }

    http.end();
    Serial.println("datasent");
}

void resetValues() {
    state = false;
    sensordata.timestamp = "NULL";
    sensordata.data.soil_moisture = 0 ;
    sensordata.data.air_temperature = 0 ;
    countertickwind = 0 ;
    sensordata.data.sunlight_intensity = 0 ;
}
