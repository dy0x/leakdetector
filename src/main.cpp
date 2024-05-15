#include <Arduino.h>
#include <LiquidCrystal.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <vector>

// WiFi
const char* WIFI_NAME = "TelstraDC683D";
const char* WIFI_PASSWORD = "9d116fd30d";
const char* TS_API = "N2II9CLQL3490VSK";
const unsigned long TS_CHANNEL = 2305005;
WiFiClient client;

// Pins
const int sensorPin = 27;
const int buttonPin = 5;
const int solenoidPin = 13;
const int LCD_RS = 22, LCD_EN = 23, LCD_D4 = 32, LCD_D5 = 33, LCD_D6 = 25,
LCD_D7 = 26;
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Define
float flowRate = 0;
std::vector<float> flowRates;
const size_t maxEntries = 10;
const float calibrationFactor = 4.5;
const unsigned int debounceInterval = 1000;

volatile bool buttonPressed = false;
volatile byte pulseCount = 0;

unsigned int sendCounter = 0;
const int sendThreshold = 30;

bool leakCheck;
unsigned int shutoffThreshold = 5;
unsigned int shutoffMin = 2;
unsigned int shutoffMax = 50;

unsigned long previousMillis = 0;
unsigned long totalMilliLitres = 0;
byte pulse1Sec = 0;

// Functions
void checkButton();
void measureFlow(float* flowRate, unsigned long* totalMilliLitres);
bool checkFlowRate();
void addFlow(float newRate, bool* leakCheck);
void displayLCD(float flowRate, unsigned long milliLitres);
void thingspeakSend(float flowRate);

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
  digitalWrite(solenoidPin,
    HIGH);  // turn the LOAD off (HIGH is the voltage level)
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
  measureFlow(&flowRate, &totalMilliLitres);
  addFlow(flowRate, &leakCheck);
  thingspeakSend(flowRate, leakCheck);
  displayLCD(flowRate, totalMilliLitres);
  delay(2000);
}

void checkButton() {
  if (buttonPressed) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Restarting!");
    Serial.println("BUTTON PRESSED!\n Opening Solenoid :)");
    buttonPressed = false;
    digitalWrite(solenoidPin,
      HIGH);  // turn the LOAD off (HIGH is the voltage level)
    flowRates.clear(); // clear the flowRate vector when the button is pressed.
  }
}

void measureFlow(float* flowRate, unsigned long* totalMilliLitres) {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > debounceInterval) {
    *flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) /
      calibrationFactor;
    unsigned int flowMilliLitres = (*flowRate / 60) * 1000;
    *totalMilliLitres += flowMilliLitres;
    pulse1Sec = pulseCount;
    pulseCount = 0;
    // previousMillis = millis();
    previousMillis = currentMillis;
  }
}

void addFlow(float newRate, bool* leakCheck) {
  if (flowRates.size() >= maxEntries) {
    flowRates.erase(flowRates.begin()); // Remove oldest element
  }
  flowRates.push_back(newRate);

  Serial.print("Last 10 Measurements: ");
  for (float rate : flowRates) {
    Serial.print(rate);
    Serial.print(", ");
  }
  Serial.println();

  bool leakCheck = checkFlowRate();
  if (leakCheck) {
    Serial.println("Shutting Solenoid !!! LEAK DETECTED !!");
  }
}

bool checkFlowRate() {
  int count = 0;
  for (float rate : flowRates) {
    if (rate > shutoffMin || rate < shutoffMax) {
      count++;
    }
    if (count >= shutoffThreshold) {
      return true;
    }
  }
  return false;
}

void thingspeakSend(float flowRate, bool leakCheck) {
  if (sendCounter == sendThreshold && leakCheck == false) {
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
