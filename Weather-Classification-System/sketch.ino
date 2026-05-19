#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "weather_classifier.h"

// ======================================================
// WIFI
// ======================================================

const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ======================================================
// THINGSPEAK
// ======================================================

const char* server = "http://api.thingspeak.com/update";
String apiKey = "FCRQZMTEXPOABQ4Z";

// ======================================================
// PINS
// ======================================================

#define DHTPIN 15
#define DHTTYPE DHT22

#define PRESSURE_PIN       1
#define WIND_PIN           2
#define PRECIPITATION_PIN  3
#define VISIBILITY_PIN     4

// RGB LED PINS
#define RED_PIN    5
#define GREEN_PIN  6
#define BLUE_PIN   16

#define BUZZER_PIN 17

// ======================================================
// OBJECTS
// ======================================================

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 20, 4);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ======================================================
// TIMER
// ======================================================

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

// ======================================================
// WEATHER PROFILES
// ======================================================

struct WeatherProfile {
  String name;
  float temp;
  float humidity;
  float pressure;
  float uv;
  float windSpeed;
  float precipitation;
  float visibility;
};

WeatherProfile profiles[] = {
  {"Sunny",  30.0, 40.0, 1025.0, 9.0, 5.0,  5.0, 10.0},
  {"Cloudy", 22.0, 65.0, 1015.0, 4.0, 12.0, 20.0, 7.0},
  {"Rainy",  17.0, 85.0, 1000.0, 1.0, 20.0, 80.0, 4.0},
  {"Snowy",   0.0, 78.0, 995.0,  0.5, 10.0, 60.0, 3.0}
};

const int NUM_PROFILES = 4;

// ======================================================
// RGB LED CONTROL
// ======================================================

void turnOffLED() {
  digitalWrite(RED_PIN, HIGH);
  digitalWrite(GREEN_PIN, HIGH);
  digitalWrite(BLUE_PIN, HIGH);
}

void setWeatherLED(String weather) {

  // OFF ALL FIRST
  turnOffLED();

  // COMMON ANODE RGB LED

  if (weather == "Sunny") {

    // YELLOW = RED + GREEN
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);

  }
  else if (weather == "Cloudy") {

    // PURPLE = RED + BLUE
    digitalWrite(RED_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);

  }
  else if (weather == "Rainy") {

    // BLUE
    digitalWrite(BLUE_PIN, LOW);

    tone(BUZZER_PIN, 1000, 300);

  }
  else if (weather == "Snowy") {

    // WHITE = ALL ON
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);

    tone(BUZZER_PIN, 1500, 500);
  }
}

// ======================================================
// DISTANCE CALCULATION
// ======================================================

float calculateDistance(
  float temp,
  float humidity,
  float pressure,
  float uv,
  float windSpeed,
  float precipitation,
  float visibility,
  WeatherProfile profile
) {

  float distance = sqrt(
    pow((temp - profile.temp), 2) +
    pow((humidity - profile.humidity), 2) +
    pow((pressure - profile.pressure), 2) +
    pow((uv - profile.uv), 2) +
    pow((windSpeed - profile.windSpeed), 2) +
    pow((precipitation - profile.precipitation), 2) +
    pow((visibility - profile.visibility), 2)
  );

  return distance;
}

// ======================================================
// WEATHER PREDICTION
// ======================================================

String predictWeather(
  float temp,
  float humidity,
  float pressure,
  float uv,
  float windSpeed,
  float precipitation,
  float visibility
) {

  float minDistance = 999999;
  String prediction = "Unknown";

  for (int i = 0; i < NUM_PROFILES; i++) {

    float distance = calculateDistance(
      temp,
      humidity,
      pressure,
      uv,
      windSpeed,
      precipitation,
      visibility,
      profiles[i]
    );

    if (distance < minDistance) {
      minDistance = distance;
      prediction = profiles[i].name;
    }
  }

  return prediction;
}

// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  // RGB PINS
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  // TURN OFF INITIALLY
  turnOffLED();

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(PRESSURE_PIN, INPUT);
  pinMode(WIND_PIN, INPUT);
  pinMode(PRECIPITATION_PIN, INPUT);
  pinMode(VISIBILITY_PIN, INPUT);

  // DHT
  dht.begin();

  // I2C
  Wire.begin(8, 9);

  // LCD
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Weather System");

  // OLED
  if (oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {

    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    oled.setCursor(0, 0);
    oled.println("Weather AI System");
    oled.display();
  }

  // WIFI
  WiFi.begin(ssid, password);

  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");

  lcd.setCursor(0, 2);
  lcd.print("WiFi Connected");

  delay(2000);

  Serial.println("\n=== SYSTEM READY ===");
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  if ((millis() - lastTime) > timerDelay) {

    // ======================================================
    // READ SENSORS
    // ======================================================

    float temp = dht.readTemperature();
    float humidity = dht.readHumidity();

    // SAFETY
    if (isnan(temp)) temp = 27;
    if (isnan(humidity)) humidity = 39;

    int pressureRaw = analogRead(PRESSURE_PIN);
    int windRaw = analogRead(WIND_PIN);
    int precipitationRaw = analogRead(PRECIPITATION_PIN);
    int visibilityRaw = analogRead(VISIBILITY_PIN);

    // ======================================================
    // CONVERT VALUES
    // ======================================================

    float pressure = map(pressureRaw, 0, 4095, 980, 1040);

    float windSpeed = map(windRaw, 0, 4095, 0, 40);

    float precipitation = map(precipitationRaw, 0, 4095, 0, 100);

    float visibility = map(visibilityRaw, 0, 4095, 1, 10);

    float uv = 10;

    // ======================================================
    // WEATHER PREDICTION
    // ======================================================

    String prediction = predictWeather(
      temp,
      humidity,
      pressure,
      uv,
      windSpeed,
      precipitation,
      visibility
    );

    // ======================================================
    // RGB LED
    // ======================================================

    setWeatherLED(prediction);

    // ======================================================
    // SERIAL MONITOR
    // ======================================================

    Serial.println("\n==============================");

    Serial.print("Temperature: ");
    Serial.println(temp);

    Serial.print("Humidity: ");
    Serial.println(humidity);

    Serial.print("Pressure: ");
    Serial.println(pressure);

    Serial.print("Wind Speed: ");
    Serial.println(windSpeed);

    Serial.print("Precipitation: ");
    Serial.println(precipitation);

    Serial.print("Visibility: ");
    Serial.println(visibility);

    Serial.print("Prediction: ");
    Serial.println(prediction);

    Serial.println("==============================");

    // ======================================================
    // LCD
    // ======================================================

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(temp);

    lcd.print(" H:");
    lcd.print(humidity);

    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(pressure);

    lcd.setCursor(0, 2);
    lcd.print("W:");
    lcd.print(windSpeed);

    lcd.setCursor(0, 3);
    lcd.print(prediction);

    // ======================================================
    // OLED
    // ======================================================

    oled.clearDisplay();

    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println("Weather Prediction");

    oled.setTextSize(2);
    oled.setCursor(0, 20);
    oled.println(prediction);

    oled.display();

    // ======================================================
    // THINGSPEAK
    // ======================================================

    if (WiFi.status() == WL_CONNECTED) {

      HTTPClient http;

      String url =
        String(server) +
        "?api_key=" + apiKey +
        "&field1=" + String(temp) +
        "&field2=" + String(humidity) +
        "&field3=" + String(pressure) +
        "&field4=" + String(uv) +
        "&field5=" + String(prediction) +   // Weather Prediction (text)
        "&field6=" + String(windSpeed) +    // Wind Speed
        "&field7=" + String(precipitation) + // Precipitation
        "&field8=" + String(visibility);    // Visibility

      http.begin(url);

      int httpCode = http.GET();

      Serial.print("ThingSpeak Response: ");
      Serial.println(httpCode);

      http.end();
    }

    lastTime = millis();
  }
}