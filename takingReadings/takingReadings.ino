#include <Wire.h>
#include "MAX30105.h"
#include "LittleFS.h"

MAX30105 particleSensor;
const int SAMPLE_SIZE = 100;

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial time to stabilize
  
  Serial.println("\n--- Non-Invasive Glucose Monitor: Safe Test Mode ---");

  // Initialize I2C Bus (SDA=D2, SCL=D1)
  Wire.begin(4, 5);

  // Initialize MAX30102 Sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("CRITICAL ERROR: MAX30102 not found! Check SDA/SCL wiring.");
    while (1) delay(10); // Safe halt without triggering Watchdog Reset
  }
  particleSensor.setup();
  Serial.println("-> Sensor Initialized Successfully.");

  // Initialize Internal LittleFS Storage
  if (!LittleFS.begin()) {
    Serial.println("CRITICAL ERROR: LittleFS Mount Failed!");
    while (1) delay(10);
  }
  Serial.println("-> Internal Flash Memory Initialized Successfully.");

  Serial.println("\nSYSTEM READY: Place finger steadily on sensor to log data...\n");
}

void loop() {
  uint32_t irValue = particleSensor.getIR();

  // If no finger is detected, just wait quietly
  if (irValue < 50000) {
    delay(500);
    return;
  }

  Serial.println("Status: Finger detected! Capturing PPG signal (Keep perfectly still for 5s)...");

  float avgRed = 0;
  float avgIR = 0;

  for (int i = 0; i < SAMPLE_SIZE; i++) {
    avgRed += particleSensor.getRed();
    avgIR += particleSensor.getIR();
    delay(50); // 50ms * 100 samples = 5 seconds
  }

  avgRed /= SAMPLE_SIZE;
  avgIR /= SAMPLE_SIZE;
  float ratio = avgRed / avgIR;

  Serial.print(">>> Captured Signal Feature (Ratio): ");
  Serial.println(ratio, 4);
  Serial.println("Enter actual blood glucose level (mg/dL) from your meter:");

  // Wait for user input in Serial Monitor
  while (Serial.available() == 0) {
    delay(100);
  }
  
  int actualGlucose = Serial.parseInt();
  if (actualGlucose > 0) {
    File file = LittleFS.open("/training.csv", "a");
    if (file) {
      file.print(ratio, 4);
      file.print(",");
      file.println(actualGlucose);
      file.close();
      
      Serial.print("SUCCESS: Data saved to internal memory! (Ratio: ");
      Serial.print(ratio, 4);
      Serial.print(" | Glucose: ");
      Serial.print(actualGlucose);
      Serial.println(")\n");
    } else {
      Serial.println("ERROR: Failed to open internal file for writing.");
    }
  }

  Serial.println("Ready for next reading in 3 seconds...");
  delay(3000);
}