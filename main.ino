#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUDP.h>
#include <time.h>
#include <sys/time.h>
#include <Wire.h>
#include <RTClib.h>
#include <MFRC522.h>
#include <SD.h>

#define W5500_CS_PIN   2   // Chip Select
#define W5500_SCK_PIN  14  // Serial Clock for W5500
#define W5500_MOSI_PIN 13  // Master Out Slave In for W5500
#define W5500_MISO_PIN 12   // Master In Slave Out for W5500

// Pin untuk RFID
#define RFID_SCK_PIN  18   // Serial Clock for RFID
#define RFID_MOSI_PIN 23   // Master Out Slave In for RFID
#define RFID_MISO_PIN 19   // Master In Slave Out for RFID
#define RST_PIN 17          // Reset pin
#define SDA_PIN 21           // SDA pin

#define SD_CS_PIN 15

#define NTP_TZ  "WIB-7"

// NTP setup
unsigned int localPort = 8888;         
char timeServer[] = "nl.pool.ntp.org"; 
const int NTP_PACKET_SIZE = 48;        
byte packetBuffer[NTP_PACKET_SIZE];     

// MAC address
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; 

EthernetUDP Udp;
EthernetClient client;

// Deep sleep
#define uS_TO_S_FACTOR 1000000  
#define TIME_TO_SLEEP  60        
#define TIME_TO_SLEEP_AFTER_S 120000 

RTC_DATA_ATTR int bootCount = 0;

// RTC Initialization
RTC_DS3231 rtc;

// RC522 Initialization
MFRC522 mfrc522(SDA_PIN, RST_PIN);  // Create MFRC522 instance

void enableEthernet()
{
  SPI.end();
  delay(100);
   SPI.begin(W5500_SCK_PIN, W5500_MISO_PIN, W5500_MOSI_PIN, W5500_CS_PIN);
  pinMode(W5500_CS_PIN, OUTPUT);
  digitalWrite(W5500_CS_PIN, HIGH); // Set CS high (idle state)

  Ethernet.init(W5500_CS_PIN);

  

  // Using DHCP to get IP address
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  } else {
    Serial.print("IP Address: ");
    Serial.println(Ethernet.localIP());
    
    // Try connecting to google.com
    if (client.connect("google.com", 80)) {
      Serial.println("Koneksi ke internet berhasil!");
      client.stop();
    } else {
      Serial.println("Koneksi ke internet gagal!");
    }
  }

  // Setup NTP
  Udp.begin(localPort);
  Serial.println("NTP Client Initialized");

  if (Ethernet.hardwareStatus() == EthernetW5500) {
    Serial.println("W5500 hardware found");
    // Test NTP sync
    syncTime_Ethernet();
  }
}

void enableRFID(){
  SPI.end();
  delay(100);
  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN); // Set RFID SPI pins
  mfrc522.PCD_Init();
  Serial.println("RFID reader initialized.");
}

void writeToFile(const char* filename, const char* data);
void readFromFile(const char* filename);

void enableSD(){
  SPI.end();
  delay(100);
  SPI.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN);

    // Inisialisasi kartu SD
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("Gagal menginisialisasi MicroSD!");
        return;
    }
    Serial.println("MicroSD berhasil diinisialisasi.");
    writeToFile("hallo.txt", "Hello, SD Card!");
    readFromFile("hallo.txt");
}

// Fungsi untuk menulis data ke file
void writeToFile(const char* filename, const char* data) {
    File dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile) {
        dataFile.println(data);
        dataFile.close();
        Serial.println("Data berhasil ditulis ke " + String(filename));
    } else {
        Serial.println("Gagal membuka file untuk menulis.");
    }
}

// Fungsi untuk membaca data dari file
void readFromFile(const char* filename) {
    File dataFile = SD.open(filename);
    if (dataFile) {
        Serial.println("Data dari " + String(filename) + ":");
        while (dataFile.available()) {
            Serial.write(dataFile.read());
        }
        dataFile.close();
    } else {
        Serial.println("Gagal membuka file untuk membaca.");
    }
}

void setup() {
  bootCount++;
  setenv("TZ", NTP_TZ, 1);
  tzset();

  Serial.begin(115200);
  Serial.println("ESP32 W5500 NTP over Ethernet, deep sleep demo");
  Serial.print("Selamat Datang");

  // Initialize SPI for W5500
 

  // Initialize RTC
  Wire.begin(25, 26);  
  if (!rtc.begin()) {
    Serial.println("RTC tidak ditemukan!");
    while (1);
  }
  
  // Set RTC time if not set
  if (rtc.lostPower()) {
    Serial.println("RTC kehilangan daya, mengatur waktu...");
    rtc.adjust(DateTime(__DATE__, __TIME__));
  }

  // Delay 10 seconds before proceeding
  delay(10000);

  // Initialize RC522
  // Initialize SPI for RFID
  
  enableRFID();
  Serial.println("Setup Complete");
}
int statusCard=0;
void loop() {
  // Deep sleep logic
  if (millis() > TIME_TO_SLEEP_AFTER_S) {
    Serial.println("Going to sleep...");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }

  // Display time from RTC
  DateTime now = rtc.now();
  Serial.print("Current RTC Time: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
  
  // RFID reading
  if (statusCard!=1)
  {
  if (mfrc522.PICC_IsNewCardPresent()) {
    Serial.println("Kartu terdeteksi!");
    if (mfrc522.PICC_ReadCardSerial()) {
      Serial.print("Card UID: ");
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i], HEX);
        if (i != mfrc522.uid.size - 1) {
          Serial.print("");
        }
      }
      Serial.println();
      mfrc522.PICC_HaltA();  // Stop reading the card
      statusCard=1;
    } else {
      Serial.println("Gagal membaca kartu!");
    }
  } else {
    Serial.println("Tidak ada kartu.");
  }
  }
  else
  {
    enableEthernet();
    enableSD();
    //GET API.... Send uuid
    //REspone
    Serial.println("SEND API");
    delay(1000);
    statusCard=0;
    enableRFID();
  }

  delay(1000);
}

void syncTime_Ethernet() {
  // Logic to sync time over NTP goes here
  // ...
}

// Send an NTP request to the time server at the given address
unsigned long sendNTPpacket(char* address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
