#include "iwi_SPI_RS0010.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);
SPI_RS0010 oled;


// Configuration parameters BEGIN
int measureTime = 500;
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
  // Initialize compass.
  mag.begin();
  Serial.begin(115200);
  // Initiate the oled:
  oled.initSPI(6, 7, 5, 9);
  oled.begin(16, 2, 1);
  oled.display();

  intro();

  // Attach Interrupts
  attachInterrupt(digitalPinToInterrupt(2), consumption_rising, RISING);
  attachInterrupt(digitalPinToInterrupt(3), distance_tick, FALLING);
}

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void intro() {
  oled.displayBuf();
  delay(3000);

  int granularity = 6;

  for (int i = 0; i < 100; i += granularity) {
    oled.fillRect(i, 0, granularity, 16, 0);
    oled.displayBuf();
  }

  oled.clearBuf();
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

int consumptionPadding(float consumption) {
  if (consumption < 10) {
    return 7;
  } else {
    return 0;
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

  if (speed > min_speed) {
    toRet = currentPerHour * 100 / (float)speed;
  } else {
    toRet = currentPerHour;
  }

  if (toRet < 100) {
    return toRet;
  } else {
    return 99.9;
  }
}

float getAverage() {
  float distance_km = (float)distance_abs / 100000;
  float toRet = 100 * fuel_injected / distance_km;

  if (toRet < 100) {
    return toRet;
  } else {
    return 99.9;
  }
}

float getHeading() {
  sensors_event_t event;
  mag.getEvent(&event);

  float heading = atan2(event.magnetic.y, event.magnetic.x);
  float declinationAngle = 0.052;
  heading += declinationAngle;

  // Correct for when signs are reversed.
  if (heading < 0)
    heading += 2 * PI;

  // Check for wrap due to addition of declination.
  if (heading > 2 * PI)
    heading -= 2 * PI;

  return heading;
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

void drawCurrent(int x, int y) {
  oled.drawLine(x + 1, y, x + 3, y, 1);
  oled.drawPixel(x + 6, y, 1);
  oled.drawLine(x, y + 1, x, y + 6, 1);
  oled.drawLine(x + 1, y + 3, x + 1, y + 6, 1);
  oled.drawLine(x + 2, y + 3, x + 2, y + 6, 1);
  oled.drawLine(x + 3, y + 3, x + 3, y + 6, 1);
  oled.drawLine(x + 4, y + 1, x + 4, y + 6, 1);
  oled.drawLine(x + 5, y + 4, x + 7, y + 6, 1);
  oled.drawLine(x + 8, y + 3, x + 8, y + 5, 1);
  oled.drawLine(x + 7, y + 1, x + 7, y + 3, 1);
}

void drawAvg(int x, int y) {
  oled.drawString(x + 1, y, "o", 1, 1, 1);
  oled.drawLine(x, y + 1, x + 6, y + 7, 1);
}

void drawPer(int x, int y) {
  oled.drawLine(x, y, x, y + 2, 1);
  oled.drawPixel(x + 1, y + 2, 1);
  oled.drawLine(x + 6, y, x, y + 6, 1);
}

void drawPerHour(int x, int y) {
  drawPer(x, y);
  oled.drawLine(x + 5, y + 4, x + 5, y + 6, 1);
  oled.drawLine(x + 7, y + 4, x + 7, y + 6, 1);
  oled.drawPixel(x + 6, y + 5, 1);
}

void drawPer100km(int x, int y) {
  drawPer(x, y);

  oled.drawPixel(x + 5, y + 4, 1);
  oled.drawLine(x + 6, y + 4, x + 6, y + 6, 1);

  oled.drawLine(x + 8, y + 4, x + 10, y + 4, 1);
  oled.drawLine(x + 8, y + 6, x + 10, y + 6, 1);
  oled.drawPixel(x + 8, y + 5, 1);
  oled.drawPixel(x + 10, y + 5, 1);

  oled.drawLine(x + 12, y + 4, x + 14, y + 4, 1);
  oled.drawLine(x + 12, y + 6, x + 14, y + 6, 1);
  oled.drawPixel(x + 12, y + 5, 1);
  oled.drawPixel(x + 14, y + 5, 1);

  oled.drawLine(x + 16, y + 4, x + 16, y + 6, 1);
  oled.drawLine(x + 17, y + 5, x + 18, y + 4, 1);
  oled.drawPixel(x + 18, y + 6, 1);

  oled.drawLine(x + 20, y + 4, x + 20, y + 6, 1);
  oled.drawLine(x + 21, y + 4, x + 22, y + 5, 1);
  oled.drawPixel(x + 23, y + 4, 1);
  oled.drawLine(x + 24, y + 4, x + 24, y + 6, 1);
}

void drawArrow(int x, int y, float angle) {
  float s = sin(angle);
  float c = cos(angle);

  int p1x = -1 * c - -7 * s;
  int p1y = -1 * s + -7 * c;

  int p2x = -4 * c - 5 * s;
  int p2y = -4 * s + 5 * c;

  int p4x = 2 * c - 5 * s;
  int p4y = 2 * s + 5 * c;

  p1x += 7;
  p1y += 7;

  p2x += 7;
  p2y += 7;
  
  p4x += 7;
  p4y += 7;

  oled.drawLine(x + p1x, y + p1y, x + p2x, y + p2y, 1);
  oled.drawLine(x + p2x, y + p2y, x + p4x, y + p4y, 1);
  oled.drawLine(x + p4x, y + p4y, x + p1x, y + p1y, 1);
}

void printDirection(int x, int y, float heading) {
  float headingDegrees = heading * 180 / M_PI;
  char *direction;

  if (headingDegrees > 315 || headingDegrees <= 45) {
    direction = "N";
  } else if (headingDegrees > 45 && headingDegrees <= 135) {
    direction = "O";
  } else if (headingDegrees > 135 && headingDegrees <= 225) {
    direction = "S";
  } else {
    direction = "W";
  }

  oled.drawString(x, y, direction, 1, 1, 1);
}

void printFuel(int x, int y) {
  oled.drawFloat(x, y, fuel_injected, 1, 1, 1, 1);
  oled.drawString(x, y + 8, "ges,", 1, 1, 1);
}

void drawAuxWindow(float heading) {
  oled.drawFastVLine(67, 0, 16, 1);
  //drawArrow(85, 1, 2 * PI - heading);
  //printDirection(77, 5, heading);
  printFuel(69, 0);
}

void loop() {
  int speed = getSpeed();
  float current = getCurrent(speed);
  float average = getAverage();
  float heading = getHeading();

  oled.clearBuf();

  // Print current
  drawCurrent(0, 0);
  oled.drawFloat(11 + consumptionPadding(current), 0, current, 1, 1, 1, 1);
  if (speed > min_speed) {
    drawPer100km(40, 0);
  } else {
    drawPerHour(40, 0);
  }

  if (average < 80) {
    // Print average
    drawAvg(0, 8);
    oled.drawFloat(11 + consumptionPadding(average), 8, average, 1, 1, 1, 1);
    drawPer100km(40, 8);
  }

  drawAuxWindow(heading);

  oled.displayBuf();
}
