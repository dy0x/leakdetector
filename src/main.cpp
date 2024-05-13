#include <Arduino.h>
#include <LiquidCrystal.h>

// Pins
const int sensorPin = 27;
const int buttonPin = 5;
const int solenoidPin = 13;
const int LCD_RS = 22, LCD_EN = 23, LCD_D4 = 32, LCD_D5 = 33, LCD_D6 = 25,
LCD_D7 = 26;

const float calibrationFactor = 4.5;
const unsigned int debounceInterval = 1000;

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

volatile boolean buttonPressed = false;
volatile byte pulseCount = 0;

byte pulse1Sec = 0;

unsigned long previousMillis = 0;
unsigned long totalMilliLitres = 0;

void checkButton();
void measureFlow();
void displayLCD(float flowRate, unsigned long milliLitres);

void IRAM_ATTR pulseCounter() { pulseCount++; }

void IRAM_ATTR handleButtonPress() { buttonPressed = true; }

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up system!");

  Serial.println("Opening Solenoid...");
  pinMode(solenoidPin, OUTPUT);
  digitalWrite(solenoidPin,
    HIGH);  // turn the LOAD off (HIGH is the voltage level)

  pinMode(buttonPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonPress, FALLING);

  lcd.begin(16, 2);
  lcd.print("System Started!");
  Serial.println("System Started!");
  delay(1500);

  pinMode(sensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sensorPin), pulseCounter, FALLING);

  lcd.clear();
}

void loop() {
  digitalWrite(solenoidPin, HIGH);
  checkButton();
  measureFlow();
}

void checkButton() {
  if (buttonPressed) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Restarting        ");  // Clearing the second line if needed
    Serial.println("BUTTON PRESSED!");
    buttonPressed = false;
  }
}

void measureFlow() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > debounceInterval) {
    float flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) /
      calibrationFactor;
    unsigned int flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;

    pulse1Sec = pulseCount;
    pulseCount = 0;
    // previousMillis = millis();
    previousMillis = currentMillis;

    displayLCD(flowRate, totalMilliLitres);
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