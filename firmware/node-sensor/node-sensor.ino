#include <GravityTDS.h>
#include <EEPROM.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <HardwareSerial.h>
GravityTDS gravityTds;
#define EEPROM_SIZE 512

String uartBuffer = "";

// Serial communication ke ESP32 aktuator
HardwareSerial SensorSerial(2);  // TX2 = GPIO17, RX2 = GPIO16

// RTC DS1302 Pin Setup
const int IO = 27;
const int SCLK = 14;
const int CE = 26;
ThreeWire myWire(IO, SCLK, CE);
RtcDS1302<ThreeWire> Rtc(myWire);

// OLED configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Sensor configuration
#define TdsSensorPin 35
#define PH_SENSOR_PIN 34
#define ONE_WIRE_BUS 4
#define VREF 5.0
#define SCOUNT 30

// Kalibrasi pH
const int filterWindowSize = 20; // Ukuran jendela moving average

float V_PH4    = 3.30;    // volt saat pH 4.01
float V_PH6_86 = 2.80;    // volt saat pH 6.86
float V_PH9    = 2.336;   // volt saat pH 9.18

float PH1 = 4.01;
float PH2 = 6.86;
float PH3 = 9.18;


float TeganganPH;
float PH_step;

float teganganBuffer[filterWindowSize];
int bufferIndex = 0;
bool bufferFilled = false;

// Variabel global sensor
float temperature = 25.0;
float nilai_tds = 420;
float nilai_ph = 7.0;

float a, b, c;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;

// Fungsi median filter untuk TDS
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++) bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  return (iFilterLen & 1) > 0 ? bTab[(iFilterLen - 1) / 2] : (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}

void handleUART() {
  while (SensorSerial.available()) {
    char c = SensorSerial.read();
    if (c == '\n') {
      prosesPerintahWaktu(uartBuffer);  // proses saat akhir baris
      uartBuffer = "";
    } else {
      uartBuffer += c;
    }
  }
}

void prosesPerintahWaktu(String data) {
  if (data.startsWith("SETTIME")) {
    int tahun, bulan, hari, jam, menit, detik;
    sscanf(data.c_str(), "SETTIME,%d,%d,%d,%d,%d,%d", &tahun, &bulan, &hari, &jam, &menit, &detik);
    
    RtcDateTime waktuBaru(tahun, bulan, hari, jam, menit, detik);
    Rtc.SetDateTime(waktuBaru);

    Serial.print("✅ RTC diatur dari Aktuator: ");
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n", tahun, bulan, hari, jam, menit, detik);
  }
}

// Fungsi pembacaan suhu
void bacaTemperatur() {
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  if (temperature == DEVICE_DISCONNECTED_C) {
    temperature = 25.0;
  }
}

// Fungsi pembacaan TDS
void bacaTDS() {
  gravityTds.setTemperature(temperature);  // Koreksi suhu
  gravityTds.update();                     // Ambil sampel dan hitung TDS
  nilai_tds = gravityTds.getTdsValue();    // Ambil nilai TDS
}

// Fungsi menyelesaikan sistem 3 persamaan linear (regresi kuadrat)
void hitungKoefisienPH(float V1, float V2, float V3, float PH1, float PH2, float PH3,
                       float &a, float &b, float &c) {
  float A[3][3] = {
    {V1 * V1, V1, 1},
    {V2 * V2, V2, 1},
    {V3 * V3, V3, 1}
  };
  float Y[3] = {PH1, PH2, PH3};

  // Eliminasi Gauss sederhana
  for (int i = 0; i < 3; i++) {
    float pivot = A[i][i];
    for (int j = 0; j < 3; j++) A[i][j] /= pivot;
    Y[i] /= pivot;

    for (int k = 0; k < 3; k++) {
      if (k == i) continue;
      float factor = A[k][i];
      for (int j = 0; j < 3; j++) A[k][j] -= factor * A[i][j];
      Y[k] -= factor * Y[i];
    }
  }

  a = Y[0];
  b = Y[1];
  c = Y[2];
}


// Fungsi pembacaan pH
float bacaPH() {
  int adc = analogRead(PH_SENSOR_PIN);
  float volt = 3.3 / 4095.0 * adc;

  // Moving average
  teganganBuffer[bufferIndex] = volt;
  bufferIndex = (bufferIndex + 1) % filterWindowSize;
  if (bufferIndex == 0) bufferFilled = true;

  float sum = 0;
  int count = bufferFilled ? filterWindowSize : bufferIndex;
  for (int i = 0; i < count; i++) sum += teganganBuffer[i];

  TeganganPH = sum / count;

  // Gunakan regresi kuadrat
  float pH = a * TeganganPH * TeganganPH + b * TeganganPH + c;

  return pH;
}


// Fungsi pembacaan waktu RTC
RtcDateTime bacaWaktuRTC() {
  return Rtc.GetDateTime();
}

// Fungsi tampilkan OLED
void tampilkanOLED(RtcDateTime now) {
  char jam[20]; snprintf(jam, sizeof(jam), "Jam : %02d:%02d:%02d", now.Hour(), now.Minute(), now.Second());
  char suhu[20]; snprintf(suhu, sizeof(suhu), "Temp: %.1f C", temperature);
  char tds[20];  snprintf(tds, sizeof(tds), "TDS : %.0f ppm", nilai_tds);
  char ph[20];   snprintf(ph, sizeof(ph), "pH  : %.2f", nilai_ph);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);  display.println(jam);
  display.setCursor(0, 16); display.println(suhu);
  display.setCursor(0, 32); display.println(tds);
  display.setCursor(0, 48); display.println(ph);
  display.display();
}

// Fungsi kirim data via serial ke ESP32 Aktuator
void kirimSerial(RtcDateTime now) {
  char serialOut[80];
  snprintf(serialOut, sizeof(serialOut), "%02d:%02d:%02d,%.1f,%.0f,%.2f",
           now.Hour(), now.Minute(), now.Second(),
           temperature, nilai_tds, nilai_ph);
  SensorSerial.println(serialOut);
  Serial.println("Data terkirim ke ESP32 Aktuator");
}

void setup() {
  Serial.begin(115200);
  SensorSerial.begin(9600, SERIAL_8N1, 16, 17);
  hitungKoefisienPH(V_PH4, V_PH6_86, V_PH9, PH1, PH2, PH3, a, b, c);
  
  EEPROM.begin(EEPROM_SIZE); 
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(3.3);               // Tegangan referensi ADC di ESP32
  gravityTds.setAdcRange(4095);          // Rentang ADC ESP32
  gravityTds.begin();                    // Inisialisasi sensor

  pinMode(PH_SENSOR_PIN, INPUT);
  sensors.begin();

  Rtc.Begin();
  RtcDateTime waktuSekarang = RtcDateTime(2025, 7, 11, 22, 17, 30);  // UBAH SESUAI JAM SAAT INI
  Rtc.SetDateTime(waktuSekarang);
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid()) Rtc.SetDateTime(compiled);
  if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);
  if (Rtc.GetDateTime() < compiled) Rtc.SetDateTime(compiled);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.display();  // Splash screen
  delay(2000);
  display.clearDisplay();
}

void loop() {
  static unsigned long analogSampleTimepoint = millis();
  static unsigned long lastSerialTime = millis();
  static unsigned long printTimepoint = millis();
  
  handleUART();
  
  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) analogBufferIndex = 0;
  }
  
  if (millis() - printTimepoint > 1000U) {
    printTimepoint = millis();
  
    // Suhu
    bacaTemperatur();
    Serial.print("Suhu: ");
    Serial.print(temperature);
    Serial.println(" °C");
  
    // TDS
    bacaTDS();
    Serial.print("TDS : ");
    Serial.print(nilai_tds);
    Serial.println(" ppm");
  
    // pH
    nilai_ph = bacaPH();
    Serial.print("pH  : ");
    Serial.println(nilai_ph, 2);

     Serial.print("tegangan pH  : ");
    Serial.println(TeganganPH);
  
    // RTC
    RtcDateTime now = bacaWaktuRTC();
  
    // OLED
    tampilkanOLED(now);
  
    // Serial kirim data tiap 5 detik
    if (millis() - lastSerialTime >= 5000U) {
      lastSerialTime = millis();
      kirimSerial(now);
    }
  
    Serial.println("----------");
  }
}
