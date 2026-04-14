#include <Arduino_BMI270_BMM150.h> // Magnetic Disturbance (Task 6C)
#include <Arduino_HS300x.h> // Humidity and Temperature (Task 7)
#include <Arduino_APDS9960.h> // Ambient Light and Color Response (Task 9C)

// Last stored sensor values
float lastHumidityReading = 0.0;    // Last known humidity (%)
float lastTemperatureReading = 0.0; // Last known temperature (°C)
float lastMagneticReading = 0.0;    // Last known magnetic field magnitude (uT)
int lastClear = 0;                  // Last known clear/brightness channel value
int lastR = 0;                      // Last known red channel value
int lastG = 0;                      // Last known green channel value
int lastB = 0;                      // Last known blue channel value

// Tolerances
static float humidityTolerance = 5.0;
static float temperatureTolerance = 0.4;
static float magneticTolerance = 15.0;
static int clearTolerance = 150;
static int rgbTolerance = 50;

// COOLDOWN TIMERS
// Track when each event last fired to prevent rapid re-triggering.
// Once an event fires, it cannot fire again until the cooldown expires.
unsigned long lastMagTime     = 0;
unsigned long lastBreathTime  = 0;
unsigned long lastLightTime   = 0;

const unsigned long MAG_COOLDOWN    = 2000;  // ms
const unsigned long BREATH_COOLDOWN = 3000;  // ms 
const unsigned long LIGHT_COOLDOWN  = 2000;  // ms


void setup() {
  Serial.begin(115200);
  delay(1500);
  
  // Magnetometer Setup (Task 6C)
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  // Humidity and Temperature Setup (Task 7)
  if (!HS300x.begin()) {
    Serial.println("Failed to initialize humidity/temperature sensor.");
    while (1);
  }

  // Ambient Light and Color Response Setup (Task 9C)
  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960 sensor.");
    while (1);
  }
}

void loop() {
  // --- 1. READ RAW VALUES ---
  // Magnetometer reading (Task 6C)
  // magnitude = sqrt(x² + y² + z²), rotation-invariant measure of field strength
  float x, y, z;
  float magneticMagnitude;
  if (IMU.magneticFieldAvailable()) {
    IMU.readMagneticField(x, y, z);
    magneticMagnitude = sqrt(x*x + y*y + z*z);
  }

  // Humidity and Temperature Reading (Task 7)
  float temperature = HS300x.readTemperature();
  float humidity = HS300x.readHumidity();

  // Ambient Light and Color Response Reading (Task 9C)
  int r, g, b, c;
  if (APDS.colorAvailable()) {
    APDS.readColor(r, g, b, c);
  }

  // --- 2. THRESHOLD INTO BINARY FLAGS ---
  // Each modality produces a 1 (changed) or 0 (no change) flag
  // by comparing the new reading to the last stored value.
  // ----------------------------------------------------------------------------------------
  // Magnetic change
  float magneticDelta = abs(magneticMagnitude - lastMagneticReading);
  int mag_shift = (magneticDelta > magneticTolerance) ? 1 : 0; 
  // ----------------------------------------------------------------------------------------
  // Humidity change 
  float humidityDelta = abs(humidity - lastHumidityReading);
  int humidityChanged = (humidityDelta > humidityTolerance) ? 1 : 0;
  
  // Temperature change
  float temperatureDelta = abs(temperature - lastTemperatureReading);
  int temperatureChanged = (temperatureDelta > temperatureTolerance) ? 1 : 0;

  // Combined flag — triggers if either changed
  int breath_or_humidity_change  = (humidityChanged || temperatureChanged)  ? 1 : 0; // CHANGE = 1, NO CHANGE = 0
  // ----------------------------------------------------------------------------------------
  // Brightness change
  int clearDelta = abs(c - lastClear);
  int clearChanged = (clearDelta > clearTolerance) ? 1 : 0;

  // Color change 
  int colorDelta = sqrt(
    pow(abs(r - lastR), 2) + 
    pow(abs(g - lastG), 2) + 
    pow(abs(b - lastB), 2)
  );
  int colorChanged = (colorDelta > rgbTolerance) ? 1 : 0;

  // Combined flag — triggers if either changed
  int light_or_color_change = (clearChanged || colorChanged) ? 1 : 0; // CHANGE = 1, NO CHANGE = 0

  // --- 3. UPDATE STORED VALUES ---
  lastMagneticReading = magneticMagnitude;
  lastHumidityReading = humidity; 
  lastTemperatureReading = temperature;
  lastR = r; lastG = g; lastB = b; lastClear = c;

  // --- 4. CLASSIFY EVENT WITH COOLDOWN DEBOUNCE ---
  // An event only triggers if its cooldown period has fully elapsed
  unsigned long now = millis();

  String label;
  if (mag_shift && (now - lastMagTime > MAG_COOLDOWN)) {
    label = "MAGNETIC_DISTURBANCE_EVENT";
    lastMagTime = now;  // reset cooldown
  }
  else if (breath_or_humidity_change && (now - lastBreathTime > BREATH_COOLDOWN)) {
    label = "BREATH_OR_WARM_AIR_EVENT";
    lastBreathTime = now;
  }
  else if (light_or_color_change && (now - lastLightTime > LIGHT_COOLDOWN)) {
    label = "LIGHT_OR_COLOR_CHANGE_EVENT";
    lastLightTime = now;
  }
  else {
    label = "BASELINE_NORMAL";
  }

  // --- 5. PRINT THREE LINES TO SERIAL MONITOR ---
  Serial.print("raw,rh="); Serial.print(lastHumidityReading);
  Serial.print(",temp="); Serial.print(lastTemperatureReading);
  Serial.print(",mag="); Serial.print(lastMagneticReading);
  Serial.print(",r="); Serial.print(r);
  Serial.print(",g="); Serial.print(g);
  Serial.print(",b="); Serial.print(b);
  Serial.print(",clear="); Serial.println(c);

  Serial.print("flags,humid_jump="); Serial.print(humidityChanged);
  Serial.print(",temp_rise="); Serial.print(temperatureChanged);
  Serial.print(",mag_shift="); Serial.print(mag_shift);
  Serial.print(",light_or_color_change="); Serial.println(light_or_color_change);

  Serial.print("event,"); 
  Serial.println(label);

  delay(1000); // Wait 1 second between classification cycles
}
