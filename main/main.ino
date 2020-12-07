#include <Wire.h> // Library for I2C communication
#include <LiquidCrystal_I2C.h> // Library for LCD

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);

// Make custom characters:
byte tank1[] = {
  B01110,
  B10001,
  B10001,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

byte tank2[] = {
  B01000,
  B00100,
  B00100,
  B00110,
  B10010,
  B01010,
  B01010,
  B00100
};

byte avg[] = {
  B00001,
  B01110,
  B10011,
  B10101,
  B10101,
  B11001,
  B01110,
  B10000
};

// Configuration parameters BEGIN
int measureTime = 1000;
// Configuration parameters END

volatile double fuel_injected = 0.0;  // in liters

volatile long consumption_time = 0;
volatile long consumption_period = 0;
volatile float dutyCycle = 0.0;
float multiplier = 93.7;              // dutyCycle * multiplier = l/h

volatile long distance_abs = 0;       // in centimeters

float min_speed = 3;                 // Speed after which there should be l/100km instead of l/h
int distance_per_tick = 24;           //in centimeters

void setup() {
  Serial.begin(115200);
  // Initiate the LCD:
  lcd.init();
  lcd.backlight();

  // Create new characters:
  lcd.createChar(0, tank1);
  lcd.createChar(1, tank2);
  lcd.createChar(2, avg);

  // Attach Interrupts
  attachInterrupt(digitalPinToInterrupt(2), consumption_rising, RISING);
  attachInterrupt(digitalPinToInterrupt(3), distance_tick, FALLING);
}

void consumption_rising() {
  consumption_period = max(1, micros() - consumption_time);
  consumption_time = micros();
  attachInterrupt(digitalPinToInterrupt(2), consumption_falling, FALLING);
}
 
void consumption_falling() {
  dutyCycle = (float)(micros() - consumption_time) / (float)consumption_period;

  // Increase total injected fuel
  fuel_injected += dutyCycle * multiplier * (float)consumption_period / 3600000000;
  attachInterrupt(digitalPinToInterrupt(2), consumption_rising, RISING);
}

void distance_tick() {
  distance_abs += distance_per_tick;
}

String consumptionPadding(float consumption) {
  if(consumption < 10) {
    return "  ";
  } else {
    return " ";
  }
}

String formatConsumption(float consumption) {
  char outStr[3];
  dtostrf(consumption,3, 1, outStr);
  return outStr;
}

String unit(int speed) {
  if(speed > min_speed) {
    return " l/100km";
  } else {
    return " l/h    ";
  }
}

float getCurrent(int speed) {
  long time_start = millis();
  float fuel_injected_start = fuel_injected;
  delay(measureTime);
  long time_end = millis() - time_start;
  float injectedPerSecond = 1000 * (fuel_injected - fuel_injected_start) / time_end;
  
  float currentPerHour = injectedPerSecond * 3600;
  float toRet = 0.0;

  if(speed > min_speed) {
    toRet = currentPerHour * 100 / (float)speed;
  } else {
    toRet = currentPerHour;
  }

  if(toRet < 100) {
    return toRet;
  } else {
    return 99.9;
  }
}

float getAverage() {
  float distance_km = (float)distance_abs / 100000;
  float toRet = 100 * fuel_injected / distance_km;

  if(toRet < 100) {
    return toRet;
  } else {
    return 99.9;
  }
}

// Get speed in km/h
int getSpeed() {
  long time_start = millis();
  long distance_start = distance_abs;
  delay(measureTime);
  long time_end = millis() - time_start;
  long distance = (float)(distance_abs - distance_start) / 100.0;   // in meters
  return (3600 * distance) / time_end;
}

void loop() {
  int speed = getSpeed();
  float current = getCurrent(speed);
  float average = getAverage();
  
  // Print current
  lcd.setCursor(0, 0);
  lcd.write(0);
  lcd.write(1);
  lcd.print(consumptionPadding(current));
  lcd.print(formatConsumption(current)); 
  lcd.print(unit(speed));

  if(distance_abs > 0) {
    // Print average
    lcd.setCursor(0, 1);
    lcd.write(2);
    lcd.print(" ");
    lcd.print(consumptionPadding(average));
    lcd.print(formatConsumption(average));
    lcd.print(" l/100km");
  }
//
//  delay(1000);
//
//  // Print speed
//  lcd.clear();
//  lcd.setCursor(0, 0);
//  lcd.print("Geschwindigkeit:");
//  lcd.setCursor(0, 1);
//  lcd.print(speed);
//  lcd.setCursor(3, 1);
//  lcd.print("km/h");
//
//  delay(1000);
}
