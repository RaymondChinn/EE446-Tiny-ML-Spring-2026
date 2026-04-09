#include <Arduino_APDS9960.h> // Proximity & Light Detection (Tasks 9A & 9C)
#include <Arduino_BMI270_BMM150.h> // Physical Motion from IMU (Task 6A) 
#include <PDM.h> // Audio (Task 5)

// Threshold values
static int micLevel= 30;
static int lightLevel = 100;
static int motionLevel= 1.0;
static int proximityLevel= 200; 

// Global persistent sensor values
int lastMicReading = 0;
int lastLightReading = 0;
float lastMotionReading = 0.0;
int lastProximityReading = 0; 
float lastAx = 0, lastAy = 0, lastAz = 0;

short sampleBuffer[256];
volatile int samplesRead = 0;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  // Audio Setup (Task 5)
  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM microphone.");
    while (1);
  }

  // IMU Setup (Task 6A)
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  // Proximity & Light Detection (Tasks 9A & 9C)
  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960 sensor.");
    while (1);
  }

}

void loop() {
  // ---- 1) Read Raw Values ----
  // Audio (Task 5) 
  if (samplesRead) {
    long sum = 0;
    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }
    lastMicReading = sum / samplesRead;
    samplesRead = 0;
  }

  // IMU (Task 6A)
  if (IMU.accelerationAvailable()) {
    float x, y, z;
    IMU.readAcceleration(x, y, z);

    // Motion = how much acceleration changed since last reading
    float delta = abs(x - lastAx) + abs(y - lastAy) + abs(z - lastAz);
    lastMotionReading = delta;

    lastAx = x;
    lastAy = y;
    lastAz = z;
  }
 
  // Proximity (Task 9A)
  if (APDS.proximityAvailable()) {
    lastProximityReading = APDS.readProximity();
  }

  // Light Detection (Task 9C)
  int r, g, b, c;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
    lastLightReading = c;
  }

  // --- 2. THRESHOLD INTO BINARY FLAGS ---
  int sound  = (lastMicReading  > micLevel)  ? 1 : 0; // NOISY = 1, QUIET = 0
  int dark   = (lastLightReading < lightLevel)  ? 1 : 0; // BRIGHT = 1, DARK = 0
  int moving = (lastMotionReading > motionLevel) ? 1 : 0; // MOVING = 1, STEADY = 0
  int near   = (lastProximityReading < proximityLevel)   ? 1 : 0; // NEAR = 1, FAR = 0

  // --- 3. CLASSIFY SITUATION ---
  String label;
  if      (!sound && !dark && !moving && !near) label = "QUIET_BRIGHT_STEADY_FAR";
  else if ( sound && !dark && !moving && !near) label = "NOISY_BRIGHT_STEADY_FAR";
  else if (!sound &&  dark && !moving &&  near) label = "QUIET_DARK_STEADY_NEAR";
  else if ( sound && !dark &&  moving &&  near) label = "NOISY_BRIGHT_MOVING_NEAR";
  else label = "OTHER_SITUATION"; // fallback — tune as needed

  // --- 4. PRINT THREE LINES ---
  Serial.print("raw,mic="); Serial.print(lastMicReading);
  Serial.print(",clear="); Serial.print(lastLightReading);
  Serial.print(",motion="); Serial.print(lastMotionReading);
  Serial.print(",prox="); Serial.println(lastProximityReading);

  Serial.print("flags,sound="); Serial.print(sound);
  Serial.print(",dark="); Serial.print(dark);
  Serial.print(",moving="); Serial.print(moving);
  Serial.print(",near="); Serial.println(near);

  Serial.print("state,"); 
  Serial.println(label);

  delay(200);
}
