#include <Arduino.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_NeoPixel.h>

// Constants
constexpr uint8_t BUFFER_SIZE = 8;
constexpr uint8_t PIN_LED = 8;
constexpr uint16_t LED_COUNT = 950;
constexpr uint8_t SENSOR1_XSHUT = 7;
constexpr uint8_t SENSOR2_XSHUT = 6;
constexpr uint8_t BRIGHTNESS = 30;
constexpr uint8_t RAINBOW_STEP = 512;
constexpr uint8_t ANIMATION_COUNT = 3;
constexpr uint16_t MIN_DISTANCE = 150;
constexpr uint16_t MAX_DISTANCE = 700;
constexpr uint16_t TIMING_BUDGET = 83000; // microseconds
constexpr unsigned long ANIMATION_INTERVAL = 4000;
constexpr unsigned long LIGHT_DURATION = 18000;
constexpr unsigned long SENSOR_BLOCK_TIME = 3000;
constexpr unsigned long MIN_LOW_DISTANCE_TIME = 3000;
constexpr unsigned long POST_CHANGE_WAIT = 3000;
constexpr unsigned long LOG_INTERVAL = 30000; // 30 seconds
constexpr float WARNING_THRESHOLD = 0.9; // 90% threshold for warnings
constexpr int TOTAL_RAM = 2048; // Total RAM for the system

// Globals
char buffer[BUFFER_SIZE];
uint8_t bufferIndex = 0;
Adafruit_VL53L0X sensor1, sensor2;
Adafruit_NeoPixel strip(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);
int distance1 = 0, distance2 = 0;
int currentAnimation1 = 0, currentAnimation2 = 0;
bool sensor1Active = false, sensor2Active = false;
unsigned long lastRainbowAngle = 0;
unsigned long animationChangeTime1 = 0, animationChangeTime2 = 0;
unsigned long lightStartTime = 0, sensorBlockTime = 0;
unsigned long lowDistanceDuration1 = 0, lowDistanceDuration2 = 0;
unsigned long lastLogTime = 0;

void initSensors();
void handleAnimations();
void animateSensor1();
void animateSensor2();
void readDistances();
void updateLights();
void switchAnimation(int &currentAnimation);
void rainbowAnimation();
void clearBuffer();
void logMemoryUsage();
void logBufferStatus();

void setup() {
  Serial.begin(9600);
  strip.setBrightness(BRIGHTNESS);
  strip.begin();
  strip.show();
  initSensors();
  logMemoryUsage(); // Log initial memory usage
}

void loop() {
  readDistances();
  handleAnimations();
  updateLights();

  // Log buffer and memory usage every LOG_INTERVAL milliseconds
  unsigned long currentTime = millis();
  if (currentTime - lastLogTime >= LOG_INTERVAL) {
    logBufferStatus();
    logMemoryUsage();
    lastLogTime = currentTime;
  }
}

void initSensors() {
  Wire.begin();

  pinMode(SENSOR1_XSHUT, OUTPUT);
  digitalWrite(SENSOR1_XSHUT, LOW);
  delay(10);
  digitalWrite(SENSOR1_XSHUT, HIGH);
  delay(10);

  if (!sensor1.begin(0x30)) {
    Serial.println("Sensor 1 initialization failed! Retrying...");
    for (int i = 0; i < 3; ++i) {
      delay(500);
      if (sensor1.begin(0x30)) {
        Serial.println("Sensor 1 initialized successfully on retry.");
        break;
      }
    }
    if (!sensor1.begin(0x30)) {
      Serial.println("Sensor 1 failed to initialize after retries. Halting...");
      while (1);
    }
  }
  sensor1.setMeasurementTimingBudgetMicroSeconds(TIMING_BUDGET);

  pinMode(SENSOR2_XSHUT, OUTPUT);
  digitalWrite(SENSOR2_XSHUT, LOW);
  delay(10);
  digitalWrite(SENSOR2_XSHUT, HIGH);
  delay(10);

  if (!sensor2.begin(0x31)) {
    Serial.println("Sensor 2 initialization failed! Retrying...");
    for (int i = 0; i < 3; ++i) {
      delay(500);
      if (sensor2.begin(0x31)) {
        Serial.print("Sensor 2 retry ");
        Serial.print(i + 1);
        Serial.println(" successful.");
        break;
      }
    }
    if (!sensor2.begin(0x31)) {
      Serial.println("Sensor 2 failed to initialize after retries. Halting...");
      while (1);
    }
  }
  sensor2.setMeasurementTimingBudgetMicroSeconds(TIMING_BUDGET);
}

void handleAnimations() {
  if (distance1 < 70) {
    if (!sensor1Active) {
      animationChangeTime1 = millis();
      sensor1Active = true;
    }
    lowDistanceDuration1 = millis() - animationChangeTime1;
    if (lowDistanceDuration1 >= MIN_LOW_DISTANCE_TIME) {
      switchAnimation(currentAnimation1);
      sensor1Active = false;
    }
  } else {
    sensor1Active = false;
    lowDistanceDuration1 = 0;
  }

  if (distance2 < 70) {
    if (!sensor2Active) {
      animationChangeTime2 = millis();
      sensor2Active = true;
    }
    lowDistanceDuration2 = millis() - animationChangeTime2;
    if (lowDistanceDuration2 >= MIN_LOW_DISTANCE_TIME) {
      switchAnimation(currentAnimation2);
      sensor2Active = false;
    }
  } else {
    sensor2Active = false;
    lowDistanceDuration2 = 0;
  }
}

void readDistances() {
  VL53L0X_RangingMeasurementData_t data1, data2;
  sensor1.rangingTest(&data1, false);
  sensor2.rangingTest(&data2, false);
  distance1 = data1.RangeMilliMeter;
  distance2 = data2.RangeMilliMeter;

  if (millis() - sensorBlockTime >= SENSOR_BLOCK_TIME) {
    if (!sensor1Active && distance1 >= MIN_DISTANCE && distance1 <= MAX_DISTANCE) {
      sensor1Active = true;
      sensor2Active = false;
      sensorBlockTime = millis();
      animateSensor1();
    }

    if (!sensor2Active && distance2 >= MIN_DISTANCE && distance2 <= MAX_DISTANCE) {
      sensor2Active = true;
      sensor1Active = false;
      sensorBlockTime = millis();
      animateSensor2();
    }
  }
}

void updateLights() {
  unsigned long currentTime = millis();

  if ((sensor1Active || sensor2Active) && lightStartTime == 0) {
    lightStartTime = currentTime;
  }

  if (sensor1Active && distance1 > MAX_DISTANCE) {
    sensor1Active = false;
  }

  if (sensor2Active && distance2 > MAX_DISTANCE) {
    sensor2Active = false;
  }

  if (currentTime - lightStartTime >= LIGHT_DURATION) {
    sensor1Active = false;
    sensor2Active = false;
    lightStartTime = 0;

    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, 0);
    }
    strip.show();
  }
}

void switchAnimation(int &currentAnimation) {
  currentAnimation = (currentAnimation + 1) % ANIMATION_COUNT;
}

void animateSensor1() {
  switch (currentAnimation1) {
    case 0:
      rainbowAnimation();
      break;
    case 1:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(50, 50, 255));
      }
      break;
    case 2:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(255, 50, 0));
      }
      break;
  }
  strip.show();
}

void animateSensor2() {
  switch (currentAnimation2) {
    case 0:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(50, 255, 50));
      }
      break;
    case 1:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(random(256), random(256), random(256)));
      }
      break;
    case 2:
      for (int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(50, 155, 155));
      }
      break;
  }
  strip.show();
}

void rainbowAnimation() {
  for (int i = 0; i < LED_COUNT; i++) {
    int hue = (lastRainbowAngle + (i * RAINBOW_STEP) / LED_COUNT) % 65536;
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue, 255, 255)));
  }
  lastRainbowAngle = (lastRainbowAngle + 256) % 65536;
  strip.show();
}

void clearBuffer() {
  bufferIndex = 0;
  memset(buffer, 0, BUFFER_SIZE);
}

void logMemoryUsage() {
  // Log free memory (for AVR boards only)
  extern int __heap_start, *__brkval;
  int v;
  int freeMemory = (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
  Serial.print("Free memory: ");
  Serial.print(freeMemory);
  Serial.println(" bytes");
  if (freeMemory < (int)(TOTAL_RAM * (1 - WARNING_THRESHOLD))) {
    Serial.println("WARNING: Free memory is critically low!");
  }
}

void logBufferStatus() {
  // Log buffer usage
  Serial.print("Buffer usage: ");
  Serial.print(bufferIndex);
  Serial.print(" / ");
  Serial.println(BUFFER_SIZE);
  if (bufferIndex >= (uint8_t)(BUFFER_SIZE * WARNING_THRESHOLD)) {
    Serial.println("WARNING: Buffer usage is near capacity!");
  }
}
