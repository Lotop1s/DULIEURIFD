#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h> 

// ==========================================
// 1. CẤU HÌNH MẠNG & GOOGLE SHEETS
// ==========================================
const char* ssid = "YOUR_WIFI_SSID";  
const char* password = "YOUR_WIFI_PASSWORD";

String Web_App_URL = "https://script.google.com/macros/s/AKfycbxFrMJK6SwqrHDr2XpQ2tuv_DX8QEACBPn6ToVXfYV2nefzeCQ_ugcNKUAJBOQxFesZ8Q/exec";

// ==========================================
// 2. CẤU HÌNH CHÂN CHO ESP32 (ĐÃ SỬA)
// ==========================================
// Sử dụng chân mặc định I2C của ESP32
#define I2C_SDA 21
#define I2C_SCL 22

// Sử dụng chân mặc định VSPI của ESP32
#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23
#define SS_PIN 5
#define RST_PIN 27 // Có thể dùng chân 27 hoặc 22 tùy ý

MFRC522 rfid(SS_PIN, RST_PIN);

LiquidCrystal_I2C lcd(0x3F, 16, 2); 

const int SERVO_PIN = 13;      // Chân PWM tốt trên ESP32
const int BUZZER_PIN = 12;      
const int LED_PIN = 14;          

Servo lockerServo; 

// ==========================================
// 3. CẤU TRÚC DỮ LIỆU
// ==========================================
#define MAX_STUDENTS 50 
struct Student {
  String id;
  char code[20]; // Tăng lên 20 để tránh tràn bộ nhớ với UID dài
  char name[50];
};

Student students[MAX_STUDENTS];
int studentCount = 0;
String uidString = "";
unsigned long lastRfidCheckTime = 0;

String currentItem = "Tu do so 1"; 

// ==========================================
// 4. HÀM ĐIỀU KHIỂN SERVO
// ==========================================
void lockDoor() {
  lockerServo.attach(SERVO_PIN); 
  lockerServo.write(10); 
  delay(500);                    
  lockerServo.detach();          
}

void unlockDoor() {
  lockerServo.attach(SERVO_PIN);
  lockerServo.write(90); 
  delay(500);
  lockerServo.detach();          
}

// ==========================================
// 5. HÀM SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  // ESP32 Servo setup
  ESP32PWM::allocateTimer(0);
  lockerServo.setPeriodHertz(50); 
  lockDoor(); 

  // Khởi động LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Khoi dong...");

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  Serial.println("\n--- DANG KET NOI WIFI ---");
  
  int counter = 0;
  while(WiFi.status() != WL_CONNECTED && counter < 20) {
    delay(1000);
    Serial.print(".");
    counter++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK!");
    lcd.clear();
    lcd.print("WiFi OK!");
  } else {
    Serial.println("\nWiFi Fail!");
    lcd.clear();
    lcd.print("WiFi Loi!");
  }
  delay(1000);

  // Khởi động RFID
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SS_PIN);
  rfid.PCD_Init(); 

  if(readDataSheet()) {
    displayReady();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Loi tai du lieu!");
    beep(5, 100);
  }
}

// ==========================================
// 6. VÒNG LẶP CHÍNH
// ==========================================
void loop() {
  if(millis() - lastRfidCheckTime > 500) {
    readUID();
    lastRfidCheckTime = millis();
  }
}

// ==========================================
// 7. CÁC HÀM XỬ LÝ
// ==========================================
void displayReady() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("He Thong San Sang");
  lcd.setCursor(0, 1);
  lcd.print("Moi Quet The...");
  digitalWrite(LED_PIN, LOW);
}

void beep(int times, int delayMs) {
  for(int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(delayMs);
    digitalWrite(BUZZER_PIN, LOW);
    if(i < times - 1) delay(delayMs);
  }
}

void readUID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidString += "0"; 
    uidString += String(rfid.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();

  Serial.println("\nQuet the: " + uidString);
  beep(1, 100); 
  lcd.clear();
  lcd.print("Dang xu ly...");

  processLocker(); 
  rfid.PICC_HaltA(); // Dừng đọc thẻ hiện tại
  rfid.PCD_StopCrypto1();
}

void processLocker() {
  char charArray[30];
  uidString.toCharArray(charArray, 30);
  
  char* studentName = getStudentNameById(charArray);
  
  if (studentName != nullptr) {
    Serial.println("Hop le. Mo tu!");
    digitalWrite(LED_PIN, HIGH); 
    unlockDoor(); 
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Xin chao:");
    lcd.setCursor(0, 1);
    
    String display_name = String(studentName);
    if(display_name.length() > 16) display_name = display_name.substring(0, 16);
    lcd.print(display_name);

    beep(2, 200); 
    sendToGoogleSheets(studentName); 

    delay(5000); 
    lockDoor();  
    displayReady();
  } else {
    Serial.println("The sai / Chua dang ky! UID: " + uidString);
    lcd.clear();
    lcd.print("THE SAI! UID:");
    lcd.setCursor(0, 1);
    lcd.print(uidString); 
    beep(3, 500); 
    delay(4000); 
    displayReady();
  }
}

void sendToGoogleSheets(char* studentName) {
  if(WiFi.status() != WL_CONNECTED) return;
  
  String Send_Data_URL = Web_App_URL + "?sts=writelog";
  Send_Data_URL += "&uid=" + uidString;
  Send_Data_URL += "&name=" + urlencode(String(studentName));
  Send_Data_URL += "&item=" + urlencode(currentItem); 

  HTTPClient http;
  http.begin(Send_Data_URL.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET(); 
  http.end();
}

bool readDataSheet() {
  if(WiFi.status() != WL_CONNECTED) return false;

  String Read_Data_URL = Web_App_URL + "?sts=read";
  HTTPClient http;
  http.begin(Read_Data_URL.c_str());
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET(); 
  studentCount = 0;

  if (httpCode > 0) {
    String payload = http.getString();
    int len = payload.length();
    char charArray[len + 1];
    payload.toCharArray(charArray, len + 1);
    
    char *token = strtok(charArray, ",");
    while (token != NULL && studentCount < MAX_STUDENTS) {
      students[studentCount].id = String(token); 
      token = strtok(NULL, ",");
      if(token) strcpy(students[studentCount].code, token); 
      token = strtok(NULL, ",");
      if(token) strcpy(students[studentCount].name, token); 
      
      studentCount++;
      token = strtok(NULL, ",");
    }
  }
  http.end();
  return (studentCount > 0);
}

String urlencode(String str) {
  String encodedString = "";
  char c;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) encodedString += c;
    else if (c == ' ') encodedString += '+';
    else {
      encodedString += '%';
      char hex[3];
      sprintf(hex, "%02X", c);
      encodedString += hex;
    }
  }
  return encodedString;
}

char* getStudentNameById(char* uid) {
  for (int i = 0; i < studentCount; i++) {
    if (strcmp(students[i].code, uid) == 0) return students[i].name;
  }
  return nullptr; 
}