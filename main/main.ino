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

volatile double fuel_injected = 0.0;   // in liters

volatile long consumption_time = 0;
volatile long consumption_period = 0;
volatile float dutyCycle = 0.0;
volatile long ontime = 0;
float multiplier = 93.7;              // dutyCycle * multiplier = l/h

volatile long distance_abs = 0;       // in centimeters
volatile long distance_time = 0;
volatile int speed = 0;

// Speed after which there should be l/100km instead of l/h
float min_speed = 10;

int distance_per_tick = 24; //in centimeters

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
  dutyCycle = min(1, (float)(micros() - consumption_time) / (float)consumption_period);

  // Calculate fuel injected in this consumption cycle
  double fuel_injected_in_cycle = dutyCycle * multiplier * (float)consumption_period / 3600000000;
  // Increase total injected fuel
  fuel_injected += fuel_injected_in_cycle;
  attachInterrupt(digitalPinToInterrupt(2), consumption_rising, RISING);
}

void distance_tick() {
  long time_between = millis()-distance_time;
  distance_time = millis();
  speed = 36.0 * (float)distance_per_tick / (float)time_between;
  distance_abs += distance_per_tick;
}

int consumptionPadding(float consumption) {
  if(consumption < 10) {
    return 1;
  } else {
    return 0;
  }
}

String formatConsumption(float consumption) {
  char outStr[3];
  dtostrf(consumption,3, 1, outStr);
  return outStr;
}

String unit() {
  if(speed > min_speed) {
    return "l/100km";
  } else {
    return "l/h    ";
  }
}

int samples = 20;

float currentAcc = 0.0;
float currentSum = 0.0;
int currentCount = 0;

float getCurrent() {
  float currentPerHour = (float)dutyCycle * multiplier;
  float toReturn = 0.0;

  if(speed > min_speed) {
    toReturn = currentPerHour * 100 / (float)speed;
  } else {
    toReturn = currentPerHour;
  }

  currentAcc += toReturn;
  currentCount++;

  if(currentCount >= samples) {
    currentSum = currentAcc / currentCount;
    currentAcc = 0.0;
    currentCount = 0;
  }

  return currentSum;
}

float getAverage() {
  float distance_km = (float)distance_abs / 100000;
  return fuel_injected / distance_km;
}

void loop() {
  lcd.clear();
  
  float current = getCurrent();
  float average = getAverage();
  
  // Print current
  lcd.setCursor(0, 0);
  lcd.write(0);
  lcd.write(1);
  lcd.setCursor(3 + consumptionPadding(current), 0);
  lcd.print(formatConsumption(current)); 
  lcd.setCursor(8,0);
  lcd.print(unit());

  // Print average
  lcd.setCursor(0, 1);
  lcd.write(2);
  lcd.setCursor(3 + consumptionPadding(average), 1);
  lcd.print(average); 
  lcd.setCursor(8,1);
  lcd.print("l/100km");

  delay(2000);

  //delay(1000);

  // Print speed
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
