#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HardwareSerial.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>  
#include "time.h"
          


#define BUTTON_PIN 23  // Pin GPIO untuk tombol reset

bool taskRunning = false;

HardwareSerial AktuatorSerial(2);

// perangkat-id
String perangkat_id = "guppy-01";
String typeTank = "";
int userTankID ;
bool bahayaTerkirim = false;


// Supabase
const String SUPABASE_KEY = "YOUR_SUPABASE_KEY";  // Pakai "Bearer " di depan!

// Token dan Chat ID Telegram
#define BOT_TOKEN "YOUR_BOT_TOKEN"
#define CHAT_ID "YOUR_CHAT_ID"

// Telegram
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// Data sensor
String inputData = "";
float tds, temp, ph;
unsigned long lastCheck = 0;
bool bahaya = false;
bool notificationsEnabled = true;  // Untuk mengatur status notifikasi
String waktu = "";
String waktuPakan = "";


//pin aktuator
#define RELAY_PUMP_PH_UP   25
#define RELAY_PUMP_PH_DOWN 26
#define RELAY_HEATER       14
#define RELAY_PELTIER      33
#define SERVO_PIN          13
#define BUZZER_PIN         32

// buzzer
unsigned long previousBuzzerMillis = 0;
const unsigned long buzzerInterval = 500; // 500 ms ON/OFF
bool buzzerState = false;
bool buzzerEnabled = true;

// aktuator
bool status_pumpphup = false;
bool status_pumpphdown = false;
bool status_heater = false;
bool status_peltier = false;
bool status_pompa_air = false;
String status_keadaan = "";
bool sudahPakan = false; 
int jamSebelumnya = -1;
String waktuPakanTerakhir = ""; 
Servo myServo;

float durasiPH= 0;

//notif tele
unsigned long lastTelegramResponseTime = 0;
const unsigned long telegramResponseInterval = 1000; // Batasi 1 pesan/detik

// Deteksi sender mati
unsigned long lastReceived = 0;
bool senderOffline = false;

//fuzzy
unsigned long startPompaPHUp = 0;
unsigned long durasiPompaPHUp = 0;
unsigned long startOffPHUp = 0;

bool pompaPHUpAktif = false;
bool pompaPHUpOffDelay = false;

unsigned long startPompaPHDown = 0;
unsigned long durasiPompaPHDown = 0;
unsigned long startOffPHDown = 0;
bool pompaPHDownAktif = false;
bool pompaPHDownOffDelay = false;
const unsigned long durasidelayOff = 30000;   // 30 detik

String namaUserTelegram = "User"; // default nilai awal
bool sedangMemproses = false;

unsigned long durasidelayOffPHUp = 0;
unsigned long durasidelayOffPHDown = 0;

unsigned long intervalKirim;

void setup() {
  Serial.begin(115200);
  AktuatorSerial.begin(9600, SERIAL_8N1, 16, 17);
  WiFiManager wm;
  //wm.resetSettings();   // <<== Reset
  String ssid_ap = "ESP32_" + perangkat_id;
  
  
  if (!wm.autoConnect(ssid_ap.c_str(), "12345678")) {
    Serial.println("WiFi Gagal, Restart");
    
    ESP.restart();
  }
  Serial.println("WiFi OK. IP: " + WiFi.localIP().toString());

  // Bypass sertifikat SSL
  secured_client.setInsecure();
  
  configTime(25200, 0, "pool.ntp.org");
  // Inisialisasi pin PIN aktuator sebagai OUTPUT
  pinMode(RELAY_PUMP_PH_UP, OUTPUT);
  pinMode(RELAY_PUMP_PH_DOWN, OUTPUT);
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_PELTIER, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  myServo.attach(SERVO_PIN);



  // Set semua relay OFF (asumsi aktif LOW, sehingga HIGH = OFF)
  digitalWrite(RELAY_PUMP_PH_UP, HIGH);
  digitalWrite(RELAY_PUMP_PH_DOWN, HIGH);
  digitalWrite(RELAY_HEATER, HIGH);
  digitalWrite(RELAY_PELTIER, HIGH);
  digitalWrite(BUZZER_PIN, LOW);  

  // Jalankan task pompa secara paralel di Core 1
  xTaskCreatePinnedToCore(
    handlePompaTask,    // Fungsi task
    "PompaHandler",     // Nama task
    4096,               // Ukuran stack
    NULL,               // Parameter
    1,                  // Prioritas
    NULL,               // Handle task
    1                   // Jalankan di Core 1
  );

  Serial.println("Aktuator siap menerima & memantau...");
  resetBotMessageCounter();
  handleStartCommand(namaUserTelegram);
  getUserTankInfo(perangkat_id);
}

void sinkronisasiWaktuRTCdanKirim() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Gagal mendapatkan waktu dari NTP.");
    return;
  }

  // Format waktu
  char waktuStr[50];
  snprintf(waktuStr, sizeof(waktuStr), "SETTIME,%04d,%02d,%02d,%02d,%02d,%02d",
           timeinfo.tm_year + 1900,
           timeinfo.tm_mon + 1,
           timeinfo.tm_mday,
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec);

  AktuatorSerial.println(waktuStr);
  Serial.print("✅ Waktu terkirim ke node sensor: ");
  bot.sendMessage(CHAT_ID, "⏱️ RTC sinkron dengan waktu NTP dan dikirim ke sensor: " + String(waktuStr), "Markdown");
  Serial.println(waktuStr);
}


void bacaDataDariSender() {
  while (AktuatorSerial.available()) {
    char c = AktuatorSerial.read();
    if (c == '\n') {
      parseData(inputData);
      inputData = "";
      lastReceived = millis();
      senderOffline = false;
    } else {
      inputData += c;
    }
  }
}

void deteksiSenderOffline() {
  if (!senderOffline && millis() - lastReceived > 15000) {
    senderOffline = true;
    Serial.println("⚠️ ESP Sender tidak aktif atau terputus!");
    bot.sendMessage(CHAT_ID, "⚠️ *ESP Sensor tidak aktif atau terputus!*", "Markdown");
  }
}

void updateDatabaseDanInformasi() {
  kirimDataSensor(perangkat_id, waktu, tds, ph, temp);
  kirimDataAktuator(perangkat_id, status_pumpphup, status_pumpphdown, status_heater, status_peltier, status_keadaan);
  getUserTankInfo(perangkat_id);
}

void handlePompaPHUp() {
  if (pompaPHUpAktif) {
    if (millis() - startPompaPHUp < durasiPompaPHUp) {
      digitalWrite(RELAY_PUMP_PH_UP, LOW);  // Pompa ON      
    } else {
      digitalWrite(RELAY_PUMP_PH_UP, HIGH); // Pompa OFF
      pompaPHUpAktif = false;
      pompaPHUpOffDelay = true;
      startOffPHUp = millis();
    }
  } else if (pompaPHUpOffDelay) {
    if (millis() - startOffPHUp >= durasidelayOffPHUp) {
      pompaPHUpOffDelay = false;
    }
  }
}

void handlePompaPHDown() {
  if (pompaPHDownAktif) {
    if (millis() - startPompaPHDown < durasiPompaPHDown) {
      digitalWrite(RELAY_PUMP_PH_DOWN, LOW);  // Pompa ON
    } else {
      digitalWrite(RELAY_PUMP_PH_DOWN, HIGH); // Pompa OFF
      pompaPHDownAktif = false;
      pompaPHDownOffDelay = true;
      startOffPHDown = millis();
    }
  } else if (pompaPHDownOffDelay) {
    if (millis() - startOffPHDown >= durasidelayOffPHDown) {
      pompaPHDownOffDelay = false;
    }
  }
}


void handlePompaTask(void * parameter) {
  for (;;) {
    handlePompaPHUp();
    handlePompaPHDown();
    vTaskDelay(100 / portTICK_PERIOD_MS); // Jalankan tiap 10ms
  }
}

void updateDatabaseTask(void * parameter) {
  updateDatabaseDanInformasi();
  taskRunning = false;
  vTaskDelete(NULL);
}

void loop() {
  bacaDataDariSender();         
  deteksiSenderOffline();       
  cekCommand();                 
  if (bahaya) {
    intervalKirim = 15000;  // 15 detik
  } else {
    intervalKirim = 60000;  // 60 detik
  }
  if (!taskRunning && !senderOffline && millis() - lastCheck > intervalKirim) {
    Serial.println("Kirim");
    xTaskCreatePinnedToCore(
      updateDatabaseTask, // Fungsi task
      "UpdateDB",         // Nama task
      8192,               // Ukuran stack
      NULL,               // Parameter
      1,                  // Prioritas
      NULL,               // Handle task
      0                   // ← PIN ke Core 0
    );
    lastCheck = millis();
  }
}

float sangatAsam(float x) {
  if (x <= 0.0 || x >= 3.6) return 0;
  else if (x == 1.8) return 1;
  else if (x < 1.8) return (x - 0.0) / (1.8 - 0.0);
  else return (3.6 - x) / (3.6 - 1.8);
}

float asamSedang(float x) {
  if (x <= 3.5 || x >= 5.6) return 0;
  else if (x == 4.5) return 1;
  else if (x < 4.5) return (x - 3.5) / (4.5 - 3.5);
  else return (5.6 - x) / (5.6 - 4.5);
}

float asamRingan(float x) {
  if (x <= 5.5 || x >= 6.6) return 0;
  else if (x == 6.0) return 1;
  else if (x < 6.0) return (x - 5.5) / (6.0 - 5.5);
  else return (6.6 - x) / (6.6 - 6.0);
}

float normal(float x) {
  if (x <= 6.5 || x >= 7.5) return 0;
  else if (x == 7.0) return 1;
  else if (x < 7.0) return (x - 6.5) / (7.0 - 6.5);
  else return (7.5 - x) / (7.5 - 7.0);
}

float basaRingan(float x) {
  if (x <= 7.4 || x >= 8.5) return 0;
  else if (x == 8.0) return 1;
  else if (x < 8.0) return (x - 7.4) / (8.0 - 7.4);
  else return (8.5 - x) / (8.5 - 8.0);
}

float basaSedang(float x) {
  if (x <= 8.4 || x >= 10.5) return 0;
  else if (x == 9.5) return 1;
  else if (x < 9.5) return (x - 8.4) / (9.5 - 8.4);
  else return (10.5 - x) / (10.5 - 9.5);
}

float sangatBasa(float x) {
  if (x <= 10.4 || x >= 14.0) return 0;
  else if (x == 12.0) return 1;
  else if (x < 12.0) return (x - 10.4) / (12.0 - 10.4);
  else return (14.0 - x) / (14.0 - 12.0);
}

// Fungsi keanggotaan durasi (output)
float durasiOff(float x) {
  return (x == 0) ? 1.0 : 0.0;
}

float sangatSingkat(float x) {
  if (x <= 30 || x >= 90) return 0.0;
  if (x == 60) return 1.0;
  if (x < 60) return (x - 30) / (60 - 30);
  return (90 - x) / (90 - 60);
}

float singkat(float x) {
  if (x <= 90 || x >= 180) return 0.0;
  if (x == 135) return 1.0;
  if (x < 135) return (x - 90) / (135 - 90);
  return (180 - x) / (180 - 135);
}

float sedang(float x) {
  if (x <= 150 || x >= 330) return 0.0;
  if (x == 240) return 1.0;
  if (x < 240) return (x - 150) / (240 - 150);
  return (330 - x) / (330 - 240);
}

float lama(float x) {
  if (x <= 300 || x >= 480) return 0.0;
  if (x == 390) return 1.0;
  if (x < 390) return (x - 300) / (390 - 300);
  return (480 - x) / (480 - 390);
}

float sangatLama(float x) {
  if (x <= 450 || x >= 600) return 0.0;
  if (x == 525) return 1.0;
  if (x < 525) return (x - 450) / (525 - 450);
  return (600 - x) / (600 - 525);
}



// Fungsi inferensi dan defuzzifikasi (metode centroid)
float inferensiDurasi(float ph) {
  float u1 = sangatAsam(ph);
  float u2 = asamSedang(ph);
  float u3 = asamRingan(ph);
  float u4 = normal(ph);
  float u5 = basaRingan(ph);
  float u6 = basaSedang(ph);
  float u7 = sangatBasa(ph);

  float totalArea = 0;
  float weightedSum = 0;
  float step = 1.0;

  for (float d = 30.0; d <= 600.0; d += step) {
    float u_d = 0;

    // Aturan Fuzzy:
    // Rule 1: sangat_asam → sangat_lama
    u_d = fmax(u_d, fmin(u1, sedang(d)));
    // Rule 2: asam_sedang → lama
    u_d = fmax(u_d, fmin(u2, lama(d)));
    // Rule 3: asam_ringan → sedang
    u_d = fmax(u_d, fmin(u3, sangatLama(d)));
    // Rule 4: normal → off
    u_d = fmax(u_d, fmin(u4, durasiOff(d)));
    // Rule 5: basa_ringan → sedang
    u_d = fmax(u_d, fmin(u5, sangatLama(d)));
    // Rule 6: basa_sedang → lama
    u_d = fmax(u_d, fmin(u6, lama(d)));
    // Rule 7: sangat_basa → sangat_lama
    u_d = fmax(u_d, fmin(u7, sedang(d)));

    weightedSum += d * u_d;
    totalArea += u_d;
  }

  if (totalArea == 0) return 0;
  return weightedSum / totalArea;
}

void parseData(String data) {
  int idx1 = data.indexOf(',');              // waktu
  int idx2 = data.indexOf(',', idx1 + 1);    // temp
  int idx3 = data.indexOf(',', idx2 + 1);    // tds

  if (idx1 != -1 && idx2 != -1 && idx3 != -1) {
    waktu = data.substring(0, idx1); // RTC untuk pakan ini jangan pake milis pake aja waktu RTC time gk perlu bulan
    temp = data.substring(idx1 + 1, idx2).toFloat();
    tds  = data.substring(idx2 + 1, idx3).toFloat();
    ph   = data.substring(idx3 + 1).toFloat();
    //ph = 4.0;
    String tempStr = String(temp, 1) + " °C";
    String tdsStr  = String(tds, 0) + " ppm";
    String phStr   = String(ph, 2);

    Serial.printf("\n[%s] Temp: %.1f°C, TDS: %.0f ppm, pH: %.2f\n", waktu.c_str(), temp, tds, ph);

    String status = "";
    bahaya = false;
    durasiPH = inferensiDurasi(ph);
    
    // Menetapkan status aktuator berdasarkan kondisi sensor
    if (ph > 7.5 && !pompaPHDownAktif && !pompaPHDownOffDelay) {
      phStr = "❗ " + phStr + " (Tidak Normal)";
      durasiPompaPHDown = 1000; // 1 detik aktif
      startPompaPHDown = millis();
      pompaPHDownAktif = true;
      pompaPHDownOffDelay = true;
      
      startOffPHDown = millis();
      durasidelayOffPHDown = durasiPH * 1000; // hasil fuzzy = waktu delay setelah ON
      status += "⚠️ pH Basa → Pompa pH Down ON (1 dtk), Delay " + String(durasiPH, 1) + " dtk\n";
      bahaya = true;
    } 

    if (ph < 6.5 && !pompaPHUpAktif && !pompaPHUpOffDelay) {
      phStr = "❗ " + phStr + " (Tidak Normal)";
      durasiPompaPHUp = 1000;
      startPompaPHUp = millis();
      pompaPHUpAktif = true;
      pompaPHUpOffDelay = true;
      startOffPHUp = millis();
      durasidelayOffPHUp = durasiPH * 1000;

      status += "⚠️ pH Asam → Pompa pH Up ON (1 dtk), Delay " + String(durasiPH, 1) + " dtk\n";
      bahaya = true;
    } 

    if (ph > 7.5){
      status_pumpphup = true;
    }else{
      status_pumpphup = false;
    }

    if (ph < 6.5){
      status_pumpphdown = true;
    }else{
      status_pumpphdown = false;
    }
    
    if (temp < 24.0) {
      tempStr = "❗ " + tempStr + " (Tidak Normal)";
      status_heater = true;
      status += "❄️ Suhu Dingin → Heater ON\n";
      bahaya = true;
    } else {
      status_heater = false;
    }

    if (temp > 28.0) {
      tempStr = "❗ " + tempStr + " (Tidak Normal)";
      status_peltier = true;
      status += "🔥 Suhu Panas → Peltier ON\n";
      bahaya = true;
    } else {
      status_peltier = false;
    }

    if (tds < 400 || tds > 600) {
    tdsStr = "❗ " + tdsStr + " (Tidak Normal)";
    status_pompa_air = true;
    status += "💧 TDS Tinggi → Perlu pergantian air\n";
    bahaya = true;
    }

    // Jika tidak ada bahaya, set status baik
    if (!bahaya) {
      status = "✅ Kualitas air baik!";
      bahaya=false;
    }

    Serial.println(status);

    //digitalWrite(RELAY_PUMP_PH_UP,   status_pumpphup   ? LOW : HIGH);
    //digitalWrite(RELAY_PUMP_PH_DOWN, status_pumpphdown ? LOW : HIGH);
    digitalWrite(RELAY_HEATER,       status_heater     ? LOW : HIGH);
    digitalWrite(RELAY_PELTIER,      status_peltier    ? LOW : HIGH);
    if (buzzerEnabled){
      handleBuzzer(bahaya);
    }else{
      digitalWrite(BUZZER_PIN, LOW);  
    }
    
    logikaPakan();

    
    // Kirim notifikasi jika bahaya dan notifikasi aktif
    if (bahaya && notificationsEnabled && !bahayaTerkirim) {
      String pesan = "🚨 *PERINGATAN IKAN GUPPY!*\n\n";
      pesan += perangkat_id + "\n";
      pesan += "📊 *TDS*: " + tdsStr + "\n";
      pesan += "🌡️ *Temp*: " + tempStr + "\n";
      pesan += "🧪 *pH*: " + phStr + "\n\n";
      pesan += status;
      bot.sendMessage(CHAT_ID, pesan, "Markdown");
      bahayaTerkirim = true;  // tandai bahwa peringatan sudah dikirim
    }
    status_keadaan = status;
    if (!bahaya && bahayaTerkirim && notificationsEnabled) {
      String pesan = "✅ *KONDISI NORMAL!*\n\n";
      pesan += perangkat_id + "\n";
      pesan += "📊 *TDS*: " + String(tds, 0) + " ppm\n";
      pesan += "🌡️ *Temp*: " + String(temp, 1) + " °C\n";
      pesan += "🧪 *pH*: " + String(ph, 2) + "\n\n";
      pesan += "Kualitas air kembali dalam batas aman. Semua aktuator dimatikan.";
      bot.sendMessage(CHAT_ID, pesan, "Markdown");
      bahayaTerkirim = false; // reset status peringatan
    }
  }
}


// Fungsi-fungsi handler untuk setiap command
void handleStartCommand(String dari) {
  String welcome = "Halo " + dari + "!\n";
  welcome += perangkat_id + " Siap Melayani\n";
  welcome += "Gunakan command berikut:\n";
  welcome += "/info - Info semua command\n";
  welcome += "/perangkat - Menampilkan informasi perangkat terakhir\n";
  welcome += "/sensor - Data sensor terakhir\n";
  welcome += "/aktuator - Status aktuator\n";
  welcome += "/beripakan - Mengaktifkan servo pakan\n";
  welcome += "/notifon - Aktifkan notifikasi\n";
  welcome += "/notifoff - Matikan notifikasi\n";
  welcome += "/buzzeron - Matikan notifikasi\n";
  welcome += "/buzzeroff - Matikan notifikasi\n";
  welcome += "/resetbot - Reset counter pesan bot\n\n";
  bot.sendMessage(CHAT_ID, welcome, "Markdown");
}

void handleInfoCommand() {
  String info = "📋 *Daftar Command yang Tersedia:*\n\n";
  info += "/perangkat - Menampilkan informasi perangkat\n";
  info += "/sensor - Menampilkan data sensor terakhir\n";
  info += "/aktuator - Menampilkan status aktuator\n";
  info += "/beripakan - Mengaktifkan servo pakan\n";
  info += "/notifon - Mengaktifkan notifikasi peringatan\n";
  info += "/notifoff - Menonaktifkan notifikasi peringatan\n";
  info += "/buzzeron - Mengaktifkan buzzer peringatan\n";
  info += "/buzzeroff - Menonaktifkan buzzer peringatan\n";
  info += "/resetbot - Reset counter pesan bot\n\n";  
  info += "Notifikasi saat ini: " + String(notificationsEnabled ? "AKTIF" : "NON-AKTIF");
  bot.sendMessage(CHAT_ID, info, "Markdown");
}

void handleSensorCommand() {
  String pesan = "📡 *Data Sensor Terakhir*\n";

  // TDS
  if (tds < 400 || tds > 600) {
    pesan += "*TDS  : " + String(tds, 0) + " ppm* ❗\n";
  } else {
    pesan += "TDS  : " + String(tds, 0) + " ppm\n";
  }

  // Temp
  if (temp < 24.0 || temp > 28.0) {
    pesan += "*Temp : " + String(temp, 1) + " °C* ❗\n";
  } else {
    pesan += "Temp : " + String(temp, 1) + " °C\n";
  }

  // pH
  if (ph < 6.5 || ph > 7.5) {
    pesan += "*pH   : " + String(ph, 2) + "* ❗\n";
  } else {
    pesan += "pH   : " + String(ph, 2) + "\n";
  }

  // Waktu
  pesan += "RTC  : [" + waktu + "] 🕖 Waktu Perangkat\n";

  // Kirim pesan
  bot.sendMessage(CHAT_ID, pesan, "Markdown");
}

void handleAktuatorCommand() {
  String status = "";
  if (ph < 6.5)  status += "Pump pH Up: ON ⚠️ pH Asam → Pompa pH Up ON (1 dtk), Delay " + String(durasiPH, 1) + " dtk\n";
  if (ph > 7.5)  status += "Pump pH Down: ON ⚠️ pH Basa → Pompa pH Down ON (1 dtk), Delay " + String(durasiPH, 1) + " dtk\n\n";
  if (temp < 24.0) status += "Heater: ON\n";
  if (temp > 28.0) status += "Peltier: ON\n";
  if (tds > 200 || tds < 600)   status += "Segera Periksa dan Ganti air\n";

  if (status == "") status = "Semua aktuator OFF, kondisi normal ✅";
  
  bot.sendMessage(CHAT_ID, "🔌 *Status Aktuator*\n" + status, "Markdown");
}

void HandlePerangkatCommand() {
  String pesan = "📟 *Informasi Perangkat*\n";
  pesan += "Perangkat     :"+ perangkat_id + "\n";
  pesan += "Tipe Tank     :"+ typeTank + "\n";
  
  if (senderOffline) {
    pesan += "Status Sensor : ❌ Offline\n";
  } else {
    pesan += "Status Sensor : ✅ Aktif\n";
  }
  bot.sendMessage(CHAT_ID, pesan, "Markdown");
}
void handleBeriPakanCommand() {
  beriPakan();
  bot.sendMessage(CHAT_ID,perangkat_id+"🍽️ Pakan telah diberikan secara manual.","");
  kirimDataLogFeeding(userTankID, true);
}

void handleNotifOnCommand() {
  notificationsEnabled = true;
  bot.sendMessage(CHAT_ID,"🔔 Notifikasi peringatan telah diaktifkan.","");
}

void handleNotifOffCommand() {
  notificationsEnabled = false;
  bot.sendMessage(CHAT_ID,"🔕 Notifikasi peringatan telah dinonaktifkan","");
}

void handleUnknownCommand() {
  String info = "❗ *Perintah tidak dikenali!*\n";
  info += "Ketik /info untuk melihat daftar command yang tersedia";
  bot.sendMessage(CHAT_ID,info, "Markdown");
}

void handleBuzerOnCommand(){
  buzzerEnabled = true;
  bot.sendMessage(CHAT_ID,"🔔 Notifikasi peringatan buzzer telah diaktifkan.","");
}

void handleBuzerOffCommand(){
  buzzerEnabled = false;
  bot.sendMessage(CHAT_ID,"🔕 Notifikasi peringatan buzzer telah dinonaktifkan.","");
}

void resetBotMessageCounter() {
  bot.last_message_received = 0;
  Serial.println("Counter pesan Telegram telah direset ke 0");
  bot.sendMessage(CHAT_ID,"🔄 Counter pesan bot telah direset", "");
}

// Fungsi utama yang dioptimasi
void cekCommand() {
  if (sedangMemproses) {
    bot.sendMessage(CHAT_ID, "⏳ Sedang memproses perintah sebelumnya. Silakan tunggu...", "");
    return;
  }
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  
  if (numNewMessages) {
    Serial.println("Menerima " + String(numNewMessages) + " pesan baru"); // Debug
    
    for (int i = 0; i < numNewMessages; i++) {
      String text = bot.messages[i].text;
      String from_name = bot.messages[i].from_name;
      int update_id = bot.messages[i].update_id;
      namaUserTelegram = from_name;
      Serial.println("Memproses command: " + text); // Debug
      sedangMemproses = true;
      Serial.println("📩 Memproses perintah: " + text);

      if (text == "/start") {
        handleStartCommand(from_name);
      } 
      else if (text == "/perangkat") {
        HandlePerangkatCommand();
      }
      else if (text == "/info") {
        handleInfoCommand();
      }
      else if (text == "/sensor") {
        handleSensorCommand();
      }
      else if (text == "/aktuator") {
        handleAktuatorCommand();
      }
      else if (text == "/beripakan") {
        handleBeriPakanCommand();
      }
      else if (text == "/notifon") {
        handleNotifOnCommand();
      }
      else if (text == "/notifoff") {
        handleNotifOffCommand();
      }
      else if (text == "/resetbot") {  // Penanganan command reset
        resetBotMessageCounter();
      }
      else if (text == "/buzzeron"){
        handleBuzerOnCommand();
      }
      else if (text == "/buzzeroff"){
        handleBuzerOffCommand();
      }
      else if (text == "/settime") {
        sinkronisasiWaktuRTCdanKirim();
      }
      else {
        handleUnknownCommand();
      }
      sedangMemproses = false;

      bot.last_message_received = update_id;
    }
    
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

void kirimDataSensor(String perangkat_id, String waktu_perangkat, float tds, float ph, float suhu) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String SUPABASE_URL = "https://your_url_supabase/rest/v1/sensor_data";
    http.begin(SUPABASE_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", SUPABASE_KEY);
    http.addHeader("Prefer", "return=representation");

    // Buat payload JSON
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["perangkat_id"] = perangkat_id;
    jsonDoc["waktu_perangkat"] = waktu_perangkat;
    jsonDoc["tds"] = tds;
    jsonDoc["ph"] = ph;
    jsonDoc["suhu"] = suhu;

    String requestBody;
    serializeJson(jsonDoc, requestBody);

    int httpResponseCode = http.POST(requestBody);
    if (httpResponseCode > 0) {
      Serial.println("Data Terkirim ✅");
      String response = http.getString();
      Serial.println(response);
    } else {
      Serial.print("Error ⛔: ");
      Serial.println(http.errorToString(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("WiFi belum terhubung");
  }
}

void kirimDataAktuator(String perangkat_id, bool status_pumpphup, bool status_pumpphdown, bool status_heater, bool status_peltier, String status_keadaan) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Ganti URL ini ke endpoint tabel `aktuator_data`
    String actuatorsURL = "https://your_url_supabase/rest/v1/aktuator_data";  // Pastikan ini URL yang benar untuk `aktuator_data`
    http.begin(actuatorsURL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + SUPABASE_KEY); // Pastikan menggunakan "Bearer" untuk Authorization header
    http.addHeader("Prefer", "return=representation");

    // Buat payload JSON
    StaticJsonDocument<256> jsonDoc;
    jsonDoc["perangkat_id"] = perangkat_id;
    // Tambahkan status relay (boolean) ke payload
    jsonDoc["pumpphup"]    = status_pumpphup;
    jsonDoc["pumpphdown"]  = status_pumpphdown;
    jsonDoc["heater"]      = status_heater;
    jsonDoc["peltier"]     = status_peltier;
    jsonDoc["keterangan"]  = status_keadaan;

    String requestBody;
    serializeJson(jsonDoc, requestBody);

    int httpResponseCode = http.POST(requestBody);
    if (httpResponseCode > 0) {
      Serial.println("✅ Data Aktuator Terkirim");
      Serial.println(http.getString());
    } else {
      Serial.print("⛔ Error kirim: ");
      Serial.println(http.errorToString(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("⛔ WiFi belum terhubung");
  }
}

void get_tankinfo(String perangkat_id) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://your_url_supabase/rest/v1/rpc/get_tank_info";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));  // Bisa dipakai juga

    String jsonData = "{\"p_perangkat_id\":\"" + perangkat_id + "\"}";
    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode == 200) {
      String response = http.getString();
      Serial.println("Tank Info:");
      Serial.println(response);

      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, response);

      if (!error && doc.size() > 0) {
        typeTank = doc[0]["type_tank"].as<String>();
        Serial.print("Jenis Akuarium: ");
        Serial.println(typeTank);
      } else {
        Serial.println("Gagal parsing JSON atau data kosong");
      }
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi belum terhubung");
  }
}

void getUserTankInfo(String perangkat_id) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://your_url_supabase/rest/v1/rpc/get_usertank_info";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));

    String jsonData = "{\"p_perangkat_id\":\"" + perangkat_id + "\"}";
    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode == 200) {
      String response = http.getString();
      Serial.println("UserTank Info Response:");
      Serial.println(response);

      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, response);

      if (!error && doc.size() > 0) {
        userTankID = doc[0]["id"].as<int>();
        typeTank = doc[0]["type_tank"].as<String>();

        Serial.print("UserTank ID: ");
        Serial.println(userTankID);
        Serial.print("Type Tank: ");
        Serial.println(typeTank);
      } else {
        Serial.println("Gagal parsing JSON atau data kosong");
      }
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi belum terhubung");
  }
}

void handleBuzzer(bool kondisiBahaya) {
  if (kondisiBahaya) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousBuzzerMillis >= buzzerInterval) {
      previousBuzzerMillis = currentMillis;
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    }
  } else {
    // Matikan buzzer jika tidak dalam kondisi bahaya
    buzzerState = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void beriPakan() {
  myServo.attach(SERVO_PIN);
  Serial.println("Feeding started.");
  myServo.write(80);  // Servo mulai berputar untuk memberi makan
  delay(5000);         // Berputar selama 5 detik untuk memberi makan
  myServo.detach();
  Serial.println("Feeding stopped.");
}

void kirimDataLogFeeding(int user_tank_id, bool status_servo){
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Ganti URL ini ke endpoint tabel `aktuator_data`
    String actuatorsURL = "https://your_url_supabase/rest/v1/feeding_log";  // Pastikan ini URL yang benar untuk `aktuator_data`
    http.begin(actuatorsURL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + SUPABASE_KEY); // Pastikan menggunakan "Bearer" untuk Authorization header
    http.addHeader("Prefer", "return=representation");

    // Buat payload JSON
    StaticJsonDocument<256> jsonDoc;
    jsonDoc["user_tank_id"] = user_tank_id;
    jsonDoc["servo_pakan"]    = status_servo;
    
    String requestBody;
    serializeJson(jsonDoc, requestBody);
    
    int httpResponseCode = http.POST(requestBody);
    if (httpResponseCode > 0) {
      Serial.println("✅ Data Log Feeding Terkirim");
      Serial.println(http.getString());
    } else {
      Serial.print("⛔ Error kirim: ");
      Serial.println(http.errorToString(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("⛔ WiFi belum terhubung");
  }
}


void logikaPakan() {
  if (waktu.length() >= 5) {
    String jammenit = waktu.substring(0, 5);
    int jam = waktu.substring(0, 2).toInt();
    int menit = waktu.substring(3, 5).toInt();
    String jam_str = waktu.substring(0, 2);
    bool waktunyaPakan = false;
    int jadwalPakan[] = {0}; // default kosong, nanti diisi sesuai tipe
    

    if (typeTank == "burayak") {
      if (jam == 7 || jam == 11 || jam == 14) waktunyaPakan = true;
    } else if (typeTank == "remaja") {
      if (jam == 7 || jam == 11) waktunyaPakan = true;
    } else if (typeTank == "dewasa") {
      if (jam == 11) waktunyaPakan = true;
    }

    // Jalankan jika dalam rentang menit 0–10
    if (waktunyaPakan && menit >= 0 && menit <= 5 && jam_str != waktuPakanTerakhir) {
      beriPakan();
      waktuPakanTerakhir = jam_str;
      Serial.println("🍽️ Pakan otomatis diberikan.");
      bot.sendMessage(CHAT_ID, "🍽️ Pakan otomatis diberikan pada jam " + jammenit, "");
      kirimDataLogFeeding(userTankID, true);
    }
  }
}
