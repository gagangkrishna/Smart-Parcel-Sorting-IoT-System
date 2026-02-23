#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#include <WiFi.h>
#include <ESP32CameraPins.h>
#include <ESP32QRCodeReader.h>
#include <esp_camera.h>
#include <HTTPClient.h>
#include "time.h"
#include "thingProperties.h"

#define SDA_PIN 13
#define SCL_PIN 15
#define flash 4

#define CAMERA_MODEL_AI_THINKER

#if defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#endif

#define WIFI_SSID "Gagan_s_Galaxy "
#define WIFI_PASSWORD "posj8411"

const char* unauthorizedBotToken = "7693189511:AAEU1Bm8uY77IibFq_-MJoUml70eXJqU65k";
const char* unauthorizedChatID = "1450588860";

#define CONVEYOR_PIN 12  

#define SERVO_MIN 102
#define SERVO_MAX 512

ESP32QRCodeReader reader;
struct QRCodeData qrCodeData;
bool isConnected = false;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

#define SERVO1_CH 0
#define SERVO2_CH 1
#define SERVO3_CH 2
#define SERVO4_CH 3
#define SERVO5_CH 4
#define SERVO6_CH 5

int gates[6] = {-1, -1, -1, -1, -1, -1};

int secondGateJump = 1;
int thirdGateJump = 1;

String lastQR = "";  // To prevent duplicate scans

void setServoAngle(uint8_t channel, int angle) {
  int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.setPWM(channel, 0, pulse);
}

void moveBelt() {
  digitalWrite(CONVEYOR_PIN, HIGH);
  delay(1020);
  digitalWrite(CONVEYOR_PIN, LOW);
  Serial.println("Conveyor belt moved");
  delay(3500);
  if (gates[0] != -1) gates[0] -= 1;
  if (gates[1] != -1) gates[1] -= 1;
  if (gates[2] != -1) gates[2] -= secondGateJump;
  if (gates[3] != -1) gates[3] -= secondGateJump;
  if (gates[4] != -1) gates[4] -= thirdGateJump;
  if (gates[5] != -1) gates[5] -= thirdGateJump;
}

void sendTelegramAlert(String message, bool unauthorized) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    const char* token = unauthorizedBotToken;
    const char* chatID = unauthorizedChatID;

    String url = "https://api.telegram.org/bot" + String(token) +
                 "/sendMessage?chat_id=" + String(chatID) +
                 "&text=" + urlEncode(message.c_str());

    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Telegram alert sent");
    } else {
      Serial.println("Failed to send Telegram alert");
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void resetAllServos() {
  for (int i = 0; i < 6; i++) {
    setServoAngle(i, 80);
  }
  Serial.println("All servos reset to 80 degrees");
}

String urlEncode(const char* str) {
  String encoded = "";
  char c;
  char code0, code1;
  for (int i = 0; i < strlen(str); i++) {
    c = str[i];
    if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

void validateLoc(String loc, int &level, int &curr_ind){
  if (loc == "Trivandrum" || loc == "Thiruvananthapuram") {
    level = 1; curr_ind = 0;
  } else if (loc == "Kollam") {
    level = 1; curr_ind = 1;  
  } else if (loc == "Alappuzha") {
    level = 2; curr_ind = 2;
  } else if (loc == "Kottayam") {
    level = 2; curr_ind = 3;
  } else if (loc == "Pathanamthitta") {
    level = 3; curr_ind = 4;
  } else if (loc == "Kochi" || loc == "Ernakulam") {
    level = 3; curr_ind = 5;
  } else {
    Serial.println("Unknown Location, package requires physical analysis...");
    level = 0; curr_ind = -1;
  }
}

void openGate(uint8_t gateA, uint8_t gateB) {
  Serial.print("Moving servos: ");
  Serial.print("Servo ");
  Serial.print(gateA);
  Serial.print(" and Servo ");
  Serial.println(gateB);

  setServoAngle(gateA, 180);
  delay(500);
  setServoAngle(gateB, 0);
  delay(500);
  setServoAngle(gateB, 80);
  delay(800);
  setServoAngle(gateA, 80);
  delay(60);
}


void logToGoogleSheet(String name, String phone, String loc) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "https://script.google.com/macros/s/AKfycbyYvmE9o-SNpot63K7RBqWr4BAqvgloI_SIrzCh6bSnE4VWI9Mp0EopuwHT6o1Lfdo/exec";
    url += "?name=" + name;
    url += "&phone=" + phone;
    url += "&location=" + loc;

    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Logged to Google Sheets successfully.");
    } else {
      Serial.println("Failed to log to Google Sheets.");
    }
    http.end();
  }
}


bool initPWMDriver() {
  Wire.beginTransmission(0x40);
  return (Wire.endTransmission() == 0);
}

void setup() {
  Serial.begin(115200);
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  reader.setup(CAMERA_MODEL_AI_THINKER);
  reader.begin();
  Serial.println("QR Reader initialized and ready");
  Wire.begin(SDA_PIN, SCL_PIN);
  pwm.begin();
  pwm.setPWMFreq(50);
  if (initPWMDriver()) {
    pwm.begin();
    pwm.setPWMFreq(50);
    Serial.println("PWM Servo Driver initialized successfully");
  } 
  else {
    Serial.println("Failed to detect PWM Servo Driver at address 0x40");
  }
  delay(10);
  pinMode(CONVEYOR_PIN, OUTPUT);
  pinMode(flash, OUTPUT);
  delay(1000);
}

void loop() {
  ArduinoCloud.update();

  if (stopSystem) {
    Serial.println("System stopped via Arduino Cloud.");
    while (stopSystem) {
      ArduinoCloud.update();
      delay(500);
    }
  }

  if (resetServos) {
    resetAllServos();
    resetServos = false;
  }

  String qrData;
  int level = 0;
  int curr_ind = -1;
  bool qrFound = false;
  bool flashOn = false;

  for (int attempt = 0; attempt < 8; attempt++) {
    Serial.print("QR Scan Attempt: ");
    Serial.println(attempt + 1);

    if (attempt > 2 && !flashOn) {
      digitalWrite(flash, HIGH);
      Serial.println("Flash turned ON");
      flashOn = true;
    }

    if (reader.receiveQrCode(&qrCodeData, 100)) {
      if (qrCodeData.valid) {
        qrData = (const char *)qrCodeData.payload;

        if (qrData == lastQR) {
          Serial.println("Duplicate QR - Ignored");
          delay(2500);
          continue;
        }

        lastQR = qrData;
        qRData = qrData;

        Serial.println("Found QRCode");
        Serial.print("Payload: ");
        Serial.println(qrData);

        int firstComma = qrData.indexOf(',');
        int secondComma = qrData.indexOf(',', firstComma + 1);
        String name = qrData.substring(0, firstComma);
        String phone = qrData.substring(firstComma + 1, secondComma);
        String loc = qrData.substring(secondComma + 1);
        logToGoogleSheet(name, phone, loc);

        String message = "Dear " + name + ", your packaged order to " + loc + " has arrived at our Amritapuri facility. Thank you for your order.";
        sendTelegramAlert(message, true);

        validateLoc(loc, level, curr_ind);
        Serial.print("Location extracted: ");
        Serial.println(loc);

        if (curr_ind >= 0) {
          if (gates[curr_ind] >= 0) {
            gates[curr_ind] += level;
          } else {
            gates[curr_ind] = level;
          }
        }

        if (gates[2] == 3 || gates[3] == 3){
          secondGateJump = 2;
        }
        else if ((gates[4] >= 4 && gates[4] <= 5) || (gates[5] >= 4 && gates[5] <= 5)){
          thirdGateJump = 2;
        }
        else if (gates[4] == 6 || gates[5] ==6){
          thirdGateJump = 3;
        }

        qrFound = true;
        break;
      }
    } else {
      Serial.println("No QR found, retrying...");
    }
    delay(2500);
  }

  if (flashOn) {
    digitalWrite(flash, LOW);
    Serial.println("Flash turned OFF");
  }

  if (qrFound) {
    moveBelt();

    if (gates[0] == 0) openGate(SERVO1_CH, SERVO2_CH);
    else if (gates[1] == 0) openGate(SERVO2_CH, SERVO1_CH);
    else if ((gates[2] == 0) || (gates[2] == 1 && secondGateJump == 2)){
      openGate(SERVO3_CH, SERVO4_CH);
      secondGateJump = 1;
    } 
    else if ((gates[3] == 0) || (gates[3] == 1 && secondGateJump == 2)){
      openGate(SERVO4_CH, SERVO3_CH);
      secondGateJump = 1;
    } 
    else if (gates[4] == 0) openGate(SERVO5_CH, SERVO6_CH);
    else if (((gates[4] == 1) || (gates[4] == 2)) && (thirdGateJump == 2)) {
      openGate(SERVO5_CH, SERVO6_CH);
      thirdGateJump = 1;
    }
    else if ((gates[4] == 3) && (thirdGateJump == 3)) {
      openGate(SERVO5_CH, SERVO6_CH);
      thirdGateJump = 2;
    }
    else if (gates[5] == 0) openGate(SERVO6_CH, SERVO6_CH);
    else if (((gates[5] == 1) || (gates[5] == 2)) && (thirdGateJump == 2)){
      openGate(SERVO6_CH, SERVO6_CH);
      thirdGateJump = 1;
    }
    else if ((gates[5] == 3) && thirdGateJump == 3) {
      openGate(SERVO6_CH, SERVO6_CH);
      thirdGateJump = 2;
    }

    resetAllServos();

    int noQRCount = 0;
    const int maxNoQRFrames = 10;

    while (noQRCount < maxNoQRFrames) {
      if (reader.receiveQrCode(&qrCodeData, 100)) {
        if (!qrCodeData.valid || String((const char*)qrCodeData.payload) != lastQR) {
          noQRCount++;
        } else {
          noQRCount = 0;
        }
      } else {
        noQRCount++;
      }
      delay(250);
    }

    Serial.println("QR code no longer visible. Ready for next scan.");
  } else {
    moveBelt();
  }

  delay(500);
  Serial.println("Done!.... Moving to next loop....");
}

void onResetServosChange() {
  // Add the code here that you want to execute when onResetServosChange() is called.
  // For example:
  Serial.println("Reset Servos Change detected!");
  // Your actual servo reset logic would go here
}

void onStopSystemChange() {
  // Add the code here that you want to execute when onStopSystemChange() is called.
  // For example:
  Serial.println("Stop System Change detected!");
  // Your actual system stop logic would go here
}