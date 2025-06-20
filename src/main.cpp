#include <Arduino.h>
#include <LiquidCrystal.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <vector>
#include "secrets.h"

// WiFi
const char* WIFI_NAME = SECRET_SSID;
const char* WIFI_PASSWORD = SECRET_PASS;
const char* TS_API = SECRET_API_KEY;
const unsigned long TS_CHANNEL = SECRET_CHANNEL;
WiFiClient client;

// Pins
constexpr int sensorPin = 27;
constexpr int buttonPin = 5;
constexpr int solenoidPin = 13;
constexpr int LCD_RS = 22, LCD_EN = 23, LCD_D4 = 32, LCD_D5 = 33, LCD_D6 = 25,
LCD_D7 = 26;
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Define
float flowRate = 0;
std::vector<float> flowRates;
constexpr size_t maxEntries = 10;
constexpr float calibrationFactor = 4.5;
constexpr unsigned int debounceInterval = 1000;

volatile bool buttonPressed = false;
volatile byte pulseCount = 0;

unsigned int sendCounter = 0;
constexpr int sendThreshold = 5;

bool leakCheck;
constexpr unsigned int shutoffThreshold = 5;
constexpr unsigned int shutoffMin = 5;
constexpr unsigned int shutoffMax = 40;

unsigned long previousMillis = 0;
unsigned long totalMilliLitres = 0;
byte pulse1Sec = 0;

// Functions
void checkButton();
void measureFlow(float& flowRate, unsigned long& totalMilliLitres);
bool checkFlowRate();
void addFlow(float newRate, bool& leakCheck);
void displayLCD(float flowRate, unsigned long milliLitres);
void thingspeakSend(float flowRate, bool leakCheck);

// Interrupts
void IRAM_ATTR pulseCounter() { pulseCount++; }
void IRAM_ATTR handleButtonPress() { buttonPressed = true; }

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up system!");
  delay(5000);

  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  Serial.printf("Connecting to WiFi: %s \n", WIFI_NAME);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Attempting to connect...");
  }
  Serial.println("Connected!\nLocal IP: " + String(WiFi.localIP()));
  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);

  pinMode(solenoidPin, OUTPUT);
  digitalWrite(solenoidPin, LOW);  // turn the LOAD off (HIGH is the voltage level)
  pinMode(buttonPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonPress, FALLING);
  Serial.println("Solenoid Opened");

  pinMode(sensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sensorPin), pulseCounter, FALLING);
  Serial.println("System Started!");
  delay(1500);
}

void loop() {
  checkButton();
  measureFlow(flowRate, totalMilliLitres);
  addFlow(flowRate, leakCheck);
  thingspeakSend(flowRate, leakCheck);
  displayLCD(flowRate, totalMilliLitres);
  delay(2500);
}

void checkButton() {
  if (buttonPressed) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Restarting!");
    Serial.println("BUTTON PRESSED!\n Opening Solenoid :)");
    buttonPressed = false;
    digitalWrite(solenoidPin, LOW);  // turn the LOAD off (HIGH is the voltage level)
    flowRates.clear(); // clear the flowRate vector when the button is pressed.
  }
}

void measureFlow(float& currentFlowRate, unsigned long& currentTotalMilliLitres) {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > debounceInterval) {
    currentFlowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) /
      calibrationFactor;
    unsigned int flowMilliLitres = (currentFlowRate / 60) * 1000;
    currentTotalMilliLitres += flowMilliLitres;
    pulse1Sec = pulseCount;
    pulseCount = 0;
    // previousMillis = millis();
    previousMillis = currentMillis;
  }
}

void addFlow(float newRate, bool& currentLeakCheck) {
  if (flowRates.size() >= maxEntries) {
    flowRates.erase(flowRates.begin()); // Remove oldest element
  }
  flowRates.push_back(newRate);

  Serial.printf("Last %zu Measurements: ", maxEntries);
  for (float rate : flowRates) {
    Serial.print(rate);
    Serial.print(", ");
  }
  Serial.println();

  currentLeakCheck = checkFlowRate();
  if (currentLeakCheck) {
    digitalWrite(solenoidPin, HIGH);  // turn the LOAD off (HIGH is the voltage level)

    Serial.println("Shutting Solenoid !!! LEAK DETECTED !!");
  }
}

bool checkFlowRate() {
  int count = 0;
  for (float rate : flowRates) {
    if (rate < shutoffMin || rate > shutoffMax) {
      count++;
    }

    if (count >= shutoffThreshold) {
      return true;
    }
  }
  return false;
}

void thingspeakSend(float flowRate, bool leakCheck) {
  if (sendCounter == sendThreshold) {
    ThingSpeak.writeField(TS_CHANNEL, 1, flowRate, TS_API);
    sendCounter = 0;
    Serial.println("Sending Data to ThingSpeak");
  }
  else {
    sendCounter++;
  }
}

void displayLCD(float flowRate, unsigned long milliLitres) {
  Serial.print("Flow rate: ");
  Serial.print(int(flowRate));  // Print the integer part of the variable
  Serial.print("L/min");
  Serial.print("\t");  // Print tab space

  lcd.setCursor(0, 0);
  lcd.print("Flow:");
  lcd.print(int(flowRate));
  lcd.print("L       ");

  // Print the cumulative total of litres flowed since starting
  Serial.print("Output Liquid Quantity: ");
  Serial.print(milliLitres);
  Serial.print("mL / ");
  Serial.print(milliLitres / 1000);
  Serial.println("L");

  lcd.setCursor(0, 1);
  lcd.print("Total:");
  lcd.print(int(milliLitres / 1000));
  lcd.print("L       ");
}
