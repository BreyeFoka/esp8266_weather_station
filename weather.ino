// ESP8266 Weather Station with Multiple Sensors
// Sensors: DHT (Temp/Humidity), Wind Speed, Gas Composition (Air Quality)
// Display: 16x2 LCD Screen with cyclic information display

#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ========== PIN DEFINITIONS ==========
#define DHTPIN D4         // DHT sensor pin (GPIO2)
#define DHTTYPE DHT22     // DHT22 (AM2302) or DHT11
#define WIND_SPEED_PIN D5 // Wind speed anemometer pin (GPIO14) - must support interrupt
#define GAS_SENSOR_PIN A0 // Gas sensor analog pin (MQ135 or similar)

// LCD Display pins (I2C - 16x2 LCD)
#define LCD_ADDR 0x27 // Default I2C address for 16x2 LCD (change if different)
#define LCD_COLS 16
#define LCD_ROWS 2

// ========== SENSOR OBJECTS ==========
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS); // Initialize 16x2 LCD

// ========== VARIABLES ==========
volatile int windPulseCount = 0;
unsigned long lastWindCheck = 0;
float windSpeed = 0.0;
float temperature = 0.0;
float humidity = 0.0;
int gasLevel = 0;
int gasQuality = 0;

// Display cycling
unsigned long lastDisplayUpdate = 0;
int currentDisplayScreen = 0; // 0: Temp/Humidity, 1: Wind Speed, 2: Air Quality
#define DISPLAY_INTERVAL 3000 // Display each screen for 3 seconds

// Wind speed calculation constants
// Typical anemometer: 1 pulse per rotation, cup diameter determines speed per rotation
#define WIND_PULSES_PER_ROTATION 1
#define WIND_CALIBRATION_FACTOR 2.4 // km/h per pulse per second (adjust for your anemometer)

// Gas sensor calibration (MQ135 or similar)
#define GAS_SENSOR_MAX_READING 1023
#define GAS_SENSOR_WARMUP_TIME 30000   // 30 seconds warmup time for MQ sensor
#define GAS_QUALITY_THRESHOLD_GOOD 400 // Adjust these based on your sensor readings
#define GAS_QUALITY_THRESHOLD_FAIR 600
#define GAS_QUALITY_THRESHOLD_POOR 800

// Calibration flag
bool gasCalibrated = false;
unsigned long calibrationStartTime = 0;

// ========== SETUP ==========
void setup()
{
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("ESP8266 Weather Station Starting...");
  Serial.println("========================================");

  // Initialize LCD
  Wire.begin(); // Start I2C communication
  lcd.init();
  lcd.backlight(); // Turn on backlight
  lcd.print("Weather Station");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  Serial.println("LCD initialized");
  delay(2000);

  // Initialize DHT sensor
  dht.begin();
  Serial.println("DHT Sensor initialized");

  // Initialize wind speed sensor (interrupt pin)
  pinMode(WIND_SPEED_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WIND_SPEED_PIN), windSpeedISR, RISING);
  Serial.println("Wind Speed Sensor initialized");

  // Initialize gas sensor
  pinMode(GAS_SENSOR_PIN, INPUT);
  Serial.println("Gas Sensor initialized");
  calibrationStartTime = millis();

  lastWindCheck = millis();
  lastDisplayUpdate = millis();

  Serial.println("All sensors initialized successfully!");
  Serial.println("Warming up gas sensor (30 seconds)...\n");

  lcd.clear();
}

// ========== MAIN LOOP ==========
void loop()
{
  // Read DHT sensor (temperature and humidity)
  readDHTSensor();

  // Calculate wind speed
  calculateWindSpeed();

  // Read gas sensor
  readGasSensor();

  // Update LCD with cyclic display
  updateLCDDisplay();

  // Display all readings to Serial
  displayReadings();

  // Wait 500ms before next reading
  delay(500);
}

// ========== SENSOR READING FUNCTIONS ==========

// Read DHT22/DHT11 temperature and humidity
void readDHTSensor()
{
  // DHT sensor can only be read every ~2 seconds
  float h = dht.readHumidity();
  float t = dht.readTemperature();     // Celsius
  float f = dht.readTemperature(true); // Fahrenheit

  // Check if readings are valid
  if (isnan(h) || isnan(t) || isnan(f))
  {
    Serial.println("WARNING: Failed to read from DHT sensor!");
    return;
  }

  humidity = h;
  temperature = t;
}

// Calculate wind speed from anemometer pulses
void calculateWindSpeed()
{
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - lastWindCheck;

  // Calculate wind speed every 5 seconds
  if (elapsedTime >= 5000)
  {
    float pulsesPerSecond = (float)windPulseCount / (elapsedTime / 1000.0);
    windSpeed = pulsesPerSecond * WIND_CALIBRATION_FACTOR;

    // Reset counter for next calculation
    windPulseCount = 0;
    lastWindCheck = currentTime;
  }
}

// Read gas composition sensor (analog)
void readGasSensor()
{
  gasLevel = analogRead(GAS_SENSOR_PIN);

  // Check if sensor is still calibrating
  if (!gasCalibrated)
  {
    if (millis() - calibrationStartTime >= GAS_SENSOR_WARMUP_TIME)
    {
      gasCalibrated = true;
      Serial.println("Gas sensor warmup complete - calibration ready!\n");
    }
    return; // Don't update quality until sensor is warm
  }

  // Determine air quality based on threshold values
  if (gasLevel < GAS_QUALITY_THRESHOLD_GOOD)
  {
    gasQuality = 0; // Good
  }
  else if (gasLevel < GAS_QUALITY_THRESHOLD_FAIR)
  {
    gasQuality = 1; // Fair
  }
  else if (gasLevel < GAS_QUALITY_THRESHOLD_POOR)
  {
    gasQuality = 2; // Poor
  }
  else
  {
    gasQuality = 3; // Very Poor
  }
}

// ========== INTERRUPT SERVICE ROUTINE ==========
void IRAM_ATTR windSpeedISR()
{
  windPulseCount++;
}

// ========== DISPLAY FUNCTIONS ==========

// Update LCD with cyclic display (rotates through different sensor readings)
void updateLCDDisplay()
{
  unsigned long currentTime = millis();

  // Switch display every DISPLAY_INTERVAL milliseconds
  if (currentTime - lastDisplayUpdate >= DISPLAY_INTERVAL)
  {
    currentDisplayScreen = (currentDisplayScreen + 1) % 3; // Cycle through 0, 1, 2
    lastDisplayUpdate = currentTime;
    lcd.clear();
  }

  // Display based on current screen
  switch (currentDisplayScreen)
  {
  case 0:
    displayScreenTemperature();
    break;
  case 1:
    displayScreenWindSpeed();
    break;
  case 2:
    displayScreenAirQuality();
    break;
  }
}

// LCD Screen 1: Temperature and Humidity
void displayScreenTemperature()
{
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temperature, 1);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(humidity, 0);
  lcd.print("%");
}

// LCD Screen 2: Wind Speed
void displayScreenWindSpeed()
{
  lcd.setCursor(0, 0);
  lcd.print("Wind Speed");

  lcd.setCursor(0, 1);
  lcd.print(windSpeed, 1);
  lcd.print(" km/h");
}

// LCD Screen 3: Air Quality (Gas Sensor)
void displayScreenAirQuality()
{
  lcd.setCursor(0, 0);
  lcd.print("Raw: ");
  lcd.print(gasLevel);

  lcd.setCursor(0, 1);

  if (!gasCalibrated)
  {
    lcd.print("Calibrating...");
  }
  else
  {
    switch (gasQuality)
    {
    case 0:
      lcd.print("Status: GOOD");
      break;
    case 1:
      lcd.print("Status: FAIR");
      break;
    case 2:
      lcd.print("Status: POOR");
      break;
    case 3:
      lcd.print("Status: VERY POOR");
      break;
    }
  }
}

void displayReadings()
{
  Serial.println("========================================");
  Serial.println("WEATHER STATION READINGS");
  Serial.println("========================================");

  // Temperature and Humidity
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Wind Speed
  Serial.print("Wind Speed: ");
  Serial.print(windSpeed);
  Serial.println(" km/h");

  // Gas/Air Quality
  Serial.print("Gas Level: ");
  Serial.print(gasLevel);
  Serial.print(" / ");
  Serial.println(GAS_SENSOR_MAX_READING);

  if (!gasCalibrated)
  {
    Serial.println("Air Quality: CALIBRATING (wait 30 sec)");
  }
  else
  {
    Serial.print("Air Quality: ");
    switch (gasQuality)
    {
    case 0:
      Serial.println("GOOD");
      break;
    case 1:
      Serial.println("FAIR");
      break;
    case 2:
      Serial.println("POOR");
      break;
    case 3:
      Serial.println("VERY POOR");
      break;
    }
  }

  Serial.println("========================================\n");
}
