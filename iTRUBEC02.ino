long lastJob1s = 0, lastJob5s = 0, lastJob30s = 0, lastJob1min = 0;
float maxtemp = 45, temp = 0;

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//Preferovaná WiFi
const char* ssid = "HOUSLEpracovna";
const char* password = "guarneri";

// nastavení propojovacích pinů RTC modulu
/*WeMos D1    vodič   RTC DS3231 modul
  -----------------------------------
  GND         seda    GND
  VCC 5 V     bila    VCC 5 V
  D14/SDA/D4  fialova SDA
  D15/SCL/D3  modra   SCL
*/
//definice adresy sbernice modulu RTC
#define DS3231_I2C_ADDRESS 0x68

// A UDP instance to let us send and receive packets over UDP
WiFiUDP ntpUDP;

//definice pro NTP klienta (název, poo-serverů, offset-s, ipdate-interval-s)
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

// nastavení propojovacích pinů Bluetooth
/*WeMOs D1    vodič   BlueTooth modul
  -----------------------------------
  GND         seda    GND
  VCC 5 V     bila    VCC 5 V
  D12/MISO/D6 fialova TXD
  D1/MOSI/D7  modra   RXD
*/
#define RX D7 //MOSI
#define TX D6 //MISO
// připojení knihovny SoftwareSerial
#include <SoftwareSerial.h>
// inicializace Bluetooth modulu z knihovny SoftwareSerial
SoftwareSerial bluetooth(TX, RX);

//Nastavení čidla DHT22
/*WeMOs D1    vodič   DHT22 AOSONG
  -----------------------------------
  GND         seda    - (vpravo)
  VCC 5 V     bila    + (vlevo)
  D8          hnědá   out (uprostřed)
*/
#define DHTtPIN D8
#define DHTtTYPE DHT22
DHT_Unified dhtt(DHTtPIN, DHTtTYPE);

//Nastavení teplotních čidel DS18B20
#define ONE_WIRE_BUS_PIN D5 // onewire pro čidla na D5
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress Probe01 = { 0x28, 0xFF, 0x13, 0x43, 0xC4, 0x17, 0x04, 0x3C };
DeviceAddress Probe02 = { 0x28, 0xFF, 0x46, 0x58, 0xC4, 0x17, 0x05, 0x89 };
DeviceAddress Probe03 = { 0x28, 0xFF, 0x7C, 0x01, 0xC4, 0x17, 0x04, 0xA5 };
DeviceAddress Probe04 = { 0x28, 0xFF, 0x5F, 0x88, 0xC3, 0x17, 0x04, 0x23 };

//korekce teplotních čidel (experimentálně zjíštěno)
float Correction1 = 0;
float Correction2 = 0.13;
float Correction3 = 0;
float Correction4 = 0.13;
float CorrectionDHT = 0;
float topeni = 0;

//Nastavení Relé
#define relePIN D10

void setup() {
  Serial.begin(9600);
  Vypis("Bootuji");
  Wire.begin();
  sensors.begin();
  timeClient.begin();

  // nastavení přesnosti senzorů DS18B20 na 11 bitů (může být 9 - 12)
  sensors.setResolution(Probe01, 11);
  sensors.setResolution(Probe02, 11);
  sensors.setResolution(Probe03, 11);
  sensors.setResolution(Probe04, 11);

  pinMode(relePIN, OUTPUT);

  // zahájení komunikace s Bluetooth modulem rychlostí 9600 baud
  bluetooth.begin(9600);
  bluetooth.println("\niTRUBEC02 přes Bluetooth...");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("iTRUBEC02");

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"amati");

  ArduinoOTA.onStart([]() {
    Vypis("OTA aktualizace start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA aktualizace konec");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA postup aktualizace: %u%%\r", (progress / (total / 100)));
    bluetooth.printf("OTA postup aktualizace: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA aktualizace - Chyba [%u]: ", error);
    bluetooth.printf("OTA aktualizace - Chyba [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Vypis("chyba autentizace");
    else if (error == OTA_BEGIN_ERROR) Vypis("chyba iniciace");
    else if (error == OTA_CONNECT_ERROR) Vypis("chyba pripojeni");
    else if (error == OTA_RECEIVE_ERROR) Vypis("chyba prijmu");
    else if (error == OTA_END_ERROR) Vypis("chyba ukonceni");
  });
  ArduinoOTA.begin();
}

byte decToBcd(byte val) { // Convert normal decimal numbers to binary coded decimal
  return ( (val / 10 * 16) + (val % 10) );
}
byte bcdToDec(byte val) { // Convert binary coded decimal to normal decimal numbers
  return ( (val / 16 * 10) + (val % 16) );
}

void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year) {
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(second)); // set seconds
  Wire.write(decToBcd(minute)); // set minutes
  Wire.write(decToBcd(hour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}

void readDS3231time(byte *second, byte *minute, byte *hour, byte *dayOfWeek, byte *dayOfMonth, byte *month, byte *year) {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}

void displayTime() {
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  // send it to the serial monitor
  Vypiss(String(hour, DEC));
  // convert the byte variable to a decimal number when displayed
  Vypiss(":");
  if (minute < 10) {
    Vypiss("0");
  }
  Vypiss(String(minute, DEC));
  Vypiss(":");
  if (second < 10) {
    Vypiss("0");
  }
  Vypiss(String(second, DEC));
  Vypiss(" ");
  Vypiss(String(dayOfMonth, DEC));
  Vypiss(".");
  Vypiss(String(month, DEC));
  Vypiss(".");
  Vypiss(String(year, DEC));
  Vypiss(" Den v tydnu: ");
  switch (dayOfWeek) {
    case 1:
      Vypis("Nedele");
      break;
    case 2:
      Vypis("Pondeli");
      break;
    case 3:
      Vypis("Utery");
      break;
    case 4:
      Vypis("Streda");
      break;
    case 5:
      Vypis("Ctvrtek");
      break;
    case 6:
      Vypis("Patek");
      break;
    case 7:
      Vypis("Sobota");
      break;
  }
}

void Vypis(String kvypsani) {  //Vypíše parametr kvypsani na seriovou linku i na bluetooth
  Serial.println(kvypsani);
  bluetooth.println(kvypsani);
}

void Vypiss(String kvypsani) { //Vypíše parametr kvypsani na seriovou linku i na bluetooth (bez odradkovani)
  Serial.print(kvypsani);
  bluetooth.print(kvypsani);
}

void AktualizujRTC() { //Zjistí přes NTP aktuální čas a aktualizuje RTC modul
  timeClient.setTimeOffset(adjustDstEurope());
  timeClient.update();
  /*Serial.println("-------------------------------");
    Serial.println(timeClient.getFormattedTime());
    Serial.println(timeClient.getFormattedDate());
    Serial.println(timeClient.getFullFormattedTime());
    Serial.println(timeClient.getDay());
    Serial.println(timeClient.getDate());
    Serial.println(timeClient.getMonth());
    Serial.println(timeClient.getYear());
    Serial.println("-------------------------------");*/
  int RTCseconds = (timeClient.getSeconds());
  int RTCminutes = (timeClient.getMinutes());
  int RTChours = (timeClient.getHours());
  int RTCday = (timeClient.getDay() + 1);
  //int RTCday = (floor((timeClient.getEpochTime()) / 86400) + 4) % 7;
  int RTCdate = (timeClient.getDate());
  int RTCmonth = (timeClient.getMonth());
  int RTCyear = (timeClient.getYear() - 2000);
  // DS3231      seconds, minutes hours, day, date, month, year
  setDS3231time(RTCseconds, RTCminutes, RTChours, RTCday, RTCdate, RTCmonth, RTCyear);
  //Vypis("RTC cas aktualizovan pres NTP.");
}

int adjustDstEurope() //funkce na zohlednění česového posunu oproti UTC včetně případného letního času
{
  // last sunday of march
  int beginDSTDate =  (31 - (5 * timeClient.getYear() / 4 + 4) % 7);
  //Serial.println(beginDSTDate);
  int beginDSTMonth = 3;
  //last sunday of october
  int endDSTDate = (31 - (5 * timeClient.getYear() / 4 + 1) % 7);
  //Serial.println(endDSTDate);
  int endDSTMonth = 10;
  // DST is valid as:
  if (((timeClient.getMonth() > beginDSTMonth) && (timeClient.getMonth() < endDSTMonth))
      || ((timeClient.getMonth() == beginDSTMonth) && (timeClient.getDay() >= beginDSTDate))
      || ((timeClient.getMonth() == endDSTMonth) && (timeClient.getDay() <= endDSTDate)))
    return 7200;  // DST europe = utc +2 hour
  else return 3600; // nonDST europe = utc +1 hour
}

void ZkusPripojitWifi() { //pokudí se připojit k WiFi a v případě úspěchu aktualizute RTC
  Vypis("Pokousim se pripojit k WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  //Vypis("Pokus o pripojeni k WiFi....");
  delay(2500);
  /*
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Vypis("Pripojeni k WiFi se nezdarilo! Zkusím se přípojit opět za minutu...");
    } else {
    Vypiss("Pripojeno k WiFi! IP adresa: ");
    Vypis(WiFi.localIP().toString());
    }
  */
}

void BlueToothHandle() { //čtení a zápis BT linky
  // proměnná pro ukládání dat z Bluetooth modulu
  byte BluetoothData;
  // kontrola Bluetooth komunikace, pokud je dostupná nová
  // zpráva, tak nám tato funkce vrátí počet jejích znaků
  if (bluetooth.available() > 0) {
    // načtení prvního znaku ve frontě do proměnné
    BluetoothData = bluetooth.read();
    // dekódování přijatého znaku
    switch (BluetoothData) {
      // každý case obsahuje dekódování jednoho znaku
      case '0':
        // v případě přijetí znaku nuly vypneme
        Vypis("Vypni....");
        break;
      case '1':
        // v případě přijetí jedničky zapneme
        Vypis("Zapni....");
        break;
      case 'a':
        // v případě přejetí znaku 'a' vypíšeme
        // čas od spuštění Arduina
        Vypiss("Cas od spusteni Arduina: ");
        Vypiss(String(millis() / 1000));
        Vypis(" vterin.");
        break;
      case 'b':
        // zde je ukázka načtení většího počtu informací,
        // po přijetí znaku 'b' tedy postupně tiskneme
        // další znaky poslané ve zprávě
        Vypis("Nacitam zpravu: ");
        BluetoothData = bluetooth.read();
        // v této smyčce zůstáváme do té doby,
        // dokud jsou nějaké znaky ve frontě
        while (bluetooth.available() > 0) {
          bluetooth.write(BluetoothData);
          // krátká pauza mezi načítáním znaků
          delay(10);
          BluetoothData = bluetooth.read();
        }
        Vypis("\n");
        break;
      case '\r':
        // přesun na začátek řádku - znak CR
        break;
      case '\n':
        // odřádkování - znak LF
        break;
      default:
        // v případě přijetí ostatních znaků vytiskneme informaci o neznámé zprávě
        Vypis("Neznamy prikaz.");
    }
  }
}

void zjistiDHT22()
{
  // Get temperature event and print its value.
  sensors_event_t eventt;
  dhtt.temperature().getEvent(&eventt);
  if (isnan(eventt.temperature)) {
    Vypis("Chyba cteni teploty z DHT22!");
  }
  delay(100);
  // Get humidity event and print its value.
  sensors_event_t eventh;
  dhtt.humidity().getEvent(&eventh);
  if (isnan(eventh.relative_humidity)) {
    Vypis("Chyba cteni vlhkosti z DHT22!");
  }
  Vypiss("Data z DHT22: ");
  Vypiss("Teplota: ");
   if (topeni < eventt.temperature)
  {
    topeni = eventt.temperature;
  }
  Vypiss(String(eventt.temperature));
  temp = eventt.temperature;
  Vypiss(" °C, Vlhkost: ");
  Vypiss(String(eventh.relative_humidity));
  Vypiss(" %");
}

void printTemperature(DeviceAddress deviceAddress, float correction)
{

  float tempC = sensors.getTempC(deviceAddress);

  tempC = tempC + correction;
  
  if (topeni < tempC)
  {
    topeni = tempC;
  }

  if (tempC == -127.00)
  {
    Serial.print("Error getting temperature  ");
  }
  else
  {
    Serial.print("C: ");
    Serial.print(tempC);
    //Serial.print(" F: ");
    //Serial.print(DallasTemperature::toFahrenheit(tempC));
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    BlueToothHandle();
  }
  //LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s-LOOP-BLOCK-1s
  if (millis() > (1000 + lastJob1s))
  {
    // kód vykonaný každou 1 vteřinu (1000 ms)


    lastJob1s = millis();
  }

  //LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s-LOOP-BLOCK-5s
  if (millis() > (5000 + lastJob5s))
  {
    // kód vykonaný každých 5 vteřin (5000 ms)

    topeni = -30;

    zjistiDHT22();
    sensors.requestTemperatures();

    Serial.print("Probe 01 temperature is:   ");
    printTemperature(Probe01, Correction1);
    Serial.println();

    Serial.print("Probe 02 temperature is:   ");
    printTemperature(Probe02, Correction2);
    Serial.println();

    Serial.print("Probe 03 temperature is:   ");
    printTemperature(Probe03, Correction3);
    Serial.println();

    Serial.print("Probe 04 temperature is:   ");
    printTemperature(Probe04, Correction4);
    Serial.println();

    if (topeni < maxtemp) {
      digitalWrite(relePIN, HIGH);
      Vypis(" - Topim");
    }
    else {
      digitalWrite(relePIN, LOW);
      Vypis(" - Netopim");
    }

    lastJob5s = millis();
  }

  //LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-
  if (millis() > (30000 + lastJob30s))
  {
    // kód vykonaný každých 30 vteřin (30000 ms)
    //AktualizujRTC();
    displayTime();

    lastJob30s = millis();
  }

  //LOOP-BLOCK-1min-LOOP-BLOCK-1min-LOOP-BLOCK-1min-LOOP-BLOCK-1min-LOOP-BLOCK-1min-LOOP-BLOCK-1min-LOOP-BLOCK-1min-LOOP-BLOCK-1min-LOOP-BLOCK-1min-LOOP-BLOCK-1min
  if (millis() > (60000 + lastJob1min))
  {
    // kód vykonaný každou 1 minutu (60000 ms)
    // Pokusime se pripojit k WiFi
    if (WiFi.status() != WL_CONNECTED) {
      ZkusPripojitWifi();
      AktualizujRTC();
    } else {
      Vypiss("Pripojeno k WiFi na IP adrese: ");
      Vypis(WiFi.localIP().toString());
    }

    lastJob1min = millis();
  }

  //LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-
}
