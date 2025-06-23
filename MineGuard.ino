#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Wire.h>
#include <MPU6050.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <math.h>

// WiFi credentials
#define WIFI_SSID "FF7C-B76C"
#define WIFI_PASSWORD "77777777"

// Firebase credentials
#define FIREBASE_HOST "mining-helmet-78c46-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "Kvb7TvtUIm8AV4hROupEdAqQALAdZ2RNki9MEGli"

// Telegram credentials
#define BOT_TOKEN "7410501396:AAFCoRGnujaCgGQth4IM4pwunCfUdIBHXpc"
#define CHAT_ID "5672531170"

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Telegram
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// Sensors
MPU6050 mpu;
int gasPin = A0;
int buzzerPin = D5;  // Buzzer connected to D5

// Alert flags
bool gasAlertSent = false;
bool fallAlertSent = false;

void setup() {
  Serial.begin(115200);
  pinMode(gasPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");

  // Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // MPU6050
  Wire.begin(D2, D1);  // SDA=D2, SCL=D1
  mpu.initialize();

  // Telegram SSL
  secured_client.setInsecure();

  // Initial read and message
  int gasValue = analogRead(gasPin);
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  float axg = ax / 16384.0;
  float ayg = ay / 16384.0;
  float azg = az / 16384.0;
  float totalAccel = sqrt(axg * axg + ayg * ayg + azg * azg);

  String initMessage = "⛑ Helmet Monitoring Started!\n"
                       "Gas Value: " + String(gasValue) + "\n"
                       "Acceleration: " + String(totalAccel, 2) + " g";
  bot.sendMessage(CHAT_ID, initMessage, "Markdown");
}

void loop() {
  // GAS Sensor
  int gasValue = analogRead(gasPin);
  String gasStatus = (gasValue > 650) ? "🔥 Gas Detected" : "✅ Normal";

  // MPU6050 - Acceleration
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  float axg = ax / 16384.0;
  float ayg = ay / 16384.0;
  float azg = az / 16384.0;
  float totalAccel = sqrt(axg * axg + ayg * ayg + azg * azg);
  String fallStatus = (totalAccel > 1) ? "🚨 Fall Detected" : "✅ Normal";

  // Serial Debug
  Serial.println("\n===== HELMET STATUS =====");
  Serial.print("Gas Value     : "); Serial.println(gasValue);
  Serial.print("Gas Status    : "); Serial.println(gasStatus);
  Serial.print("Acceleration  : "); Serial.println(totalAccel);
  Serial.print("Fall Status   : "); Serial.println(fallStatus);
  Serial.println("==========================");

  // Firebase
  Firebase.setInt(fbdo, "/Helmet/gasValue", gasValue);
  Firebase.setString(fbdo, "/Helmet/gasStatus", gasStatus);
  Firebase.setFloat(fbdo, "/Helmet/acceleration", totalAccel);
  Firebase.setString(fbdo, "/Helmet/fallStatus", fallStatus);

  // Telegram + Buzzer for Gas
  if (gasStatus == "🔥 Gas Detected" && !gasAlertSent) {
    String msg = "🚨 Gas Alert!\nGas Level: " + String(gasValue) + "";
    bot.sendMessage(CHAT_ID, msg, "Markdown");
    digitalWrite(buzzerPin, HIGH);  // Buzzer ON
    gasAlertSent = true;
  } else if (gasStatus == "✅ Normal") {
    gasAlertSent = false;
  }

  // Telegram + Buzzer for Fall
  if (fallStatus == "🚨 Fall Detected" && !fallAlertSent) {
    String msg = "⚠ Fall Detected!\nAcceleration: " + String(totalAccel, 2) + " g";
    bot.sendMessage(CHAT_ID, msg, "Markdown");
    digitalWrite(buzzerPin, HIGH);  // Buzzer ON
    fallAlertSent = true;
  } else if (fallStatus == "✅ Normal") {
    fallAlertSent = false;
  }

  // Turn off buzzer if everything normal
  if (gasStatus == "✅ Normal" && fallStatus == "✅ Normal") {
    digitalWrite(buzzerPin, LOW); // Buzzer OFF
  }

  delay(5000);
}
