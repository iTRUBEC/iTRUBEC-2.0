long lastJob1s = 0, lastJob5s = 0, lastJob30s = 0, lastJob1min = 0;
float maxtemp = 49, temp = 0, mintemp = 0, humidity = 0;
String record = "";
byte tsec, tmin, thour, tdayOfWeek, tdayOfMonth, tmonth, tyear;

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoOTA.h>
#include <Wire.h>
//#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
//#include <OneWire.h>
#include <DallasTemperature.h>
//#include <SPI.h>
#include <SD.h>

File myFile;
const int chipSelect = D2; // SD Card Reader CS

//Preferovaná WiFi
const char* ssid = "iTRUBEC";
const char* password = "1234567890";

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

//definice pro NTP klienta (název, pool-serverů, offset-s, ipdate-interval-s)
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

// nastavení propojovacích pinů Bluetooth
/*WeMOs D1    vodič   BlueTooth modul
  -----------------------------------
  GND         seda    GND
  VCC 5 V     bila    VCC 5 V
  D0 RX       fialova TXD
  D1 TX       modra   RXD
*/



//Nastavení čidla DHT22
/*WeMOs D1    vodič   DHT22 AOSONG
  -----------------------------------
  GND         seda    - (vpravo)
  VCC 5 V     bila    + (vlevo)
  D9          hnědá   out (uprostřed)
*/
#define DHTtPIN D9
#define DHTtTYPE DHT22
DHT dht(DHTtPIN, DHTtTYPE);
//DHT_Unified dhtt(DHTtPIN, DHTtTYPE);

//Nastavení teplotních čidel DS18B20
#define ONE_WIRE_BUS_PIN D8 // onewire pro čidla na D5
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
  Serial.println("Bootuji");
  Wire.begin();
  sensors.begin();
  timeClient.begin();
  dht.begin();

  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("SD Card initialization FAILED!");
    //return;
  } else {
    Serial.println("SD Card initialization OK.");
  }

  // nastavení přesnosti senzorů DS18B20 na 11 bitů (může být 9 - 12)
  sensors.setResolution(Probe01, 11);
  sensors.setResolution(Probe02, 11);
  sensors.setResolution(Probe03, 11);
  sensors.setResolution(Probe04, 11);

  pinMode(relePIN, OUTPUT);

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("iTRUBEC02");

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"itubec");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA aktualizace start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA aktualizace konec");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA postup aktualizace: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA aktualizace - Chyba [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("chyba autentizace");
    else if (error == OTA_BEGIN_ERROR) Serial.println("chyba iniciace");
    else if (error == OTA_CONNECT_ERROR) Serial.println("chyba pripojeni");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("chyba prijmu");
    else if (error == OTA_END_ERROR) Serial.println("chyba ukonceni");
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
  Serial.print(String(hour, DEC));
  // convert the byte variable to a decimal number when displayed
  Serial.print(":");
  if (minute < 10) {
    Serial.print("0");
  }
  Serial.print(String(minute, DEC));
  Serial.print(":");
  if (second < 10) {
    Serial.print("0");
  }
  Serial.print(String(second, DEC));
  Serial.print(" ");
  Serial.print(String(dayOfMonth, DEC));
  Serial.print(".");
  Serial.print(String(month, DEC));
  Serial.print(".");
  Serial.print(String(year, DEC));
  Serial.print(" Den v tydnu: ");
  switch (dayOfWeek) {
    case 1:
      Serial.println("Nedele");
      break;
    case 2:
      Serial.println("Pondeli");
      break;
    case 3:
      Serial.println("Utery");
      break;
    case 4:
      Serial.println("Streda");
      break;
    case 5:
      Serial.println("Ctvrtek");
      break;
    case 6:
      Serial.println("Patek");
      break;
    case 7:
      Serial.println("Sobota");
      break;
  }
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
  //Serial.println("RTC cas aktualizovan pres NTP.");
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
  Serial.println("Pokousim se pripojit k WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Pokus o pripojeni k WiFi....");
  delay(2500);

  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Pripojeni k WiFi se nezdarilo! Zkusím se přípojit opět za minutu...");
  } else {
    Serial.print("Pripojeno k WiFi! IP adresa: ");
    Serial.println(WiFi.localIP().toString());
  }
}

void zjistiDHT22()
{
  // Get temperature event and print its value.
  //sensors_event_t eventt;
  //dhtt.temperature().getEvent(&eventt);
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  //if (isnan(eventt.temperature)) {
  //  Serial.println("Error reading temperature from DHT22!");
  //}
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
  }
  // Get humidity event and print its value.
  //sensors_event_t eventh;
  //dhtt.humidity().getEvent(&eventh);
  //if (isnan(eventh.relative_humidity)) {
  //  Serial.println("Error reading humidity from DHT22!");
  //}
  Serial.print("DHT22 sensor: ");
  Serial.print("Temperature: ");
  if (topeni < t)
  {
    topeni = t;
  }
  Serial.print(String(t));
  temp = t;
  Serial.print(" °C, Humidity: ");
  Serial.print(String(h));
  humidity = h;
  Serial.println(" %");
  delay(170);
  Serial.print("t5=");
  Serial.println(String(t));
  delay(170);
  Serial.print("v1=");
  Serial.println(String(h));
  delay(170);
}

float myTemperature(DeviceAddress deviceAddress, float correction)
{
  float tempC = sensors.getTempC(deviceAddress);
  tempC = tempC + correction;
  if (topeni < tempC)  {
    topeni = tempC;
  }
  if (mintemp < tempC) {
    mintemp = tempC;
  }
  if (tempC == -127.00) {
    return (-200);
  }
  else {
    return (tempC);
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
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
    mintemp = -100;
    record = ""; //record to be stored to SD

    readDS3231time(&tsec, &tmin, &thour, &tdayOfWeek, &tdayOfMonth, &tmonth, &tyear);

    record = String(tdayOfMonth, DEC);
    record = record + ".";
    record = record + String(tdayOfMonth, DEC);
    record = record + ".";
    record = record + String(tyear, DEC);
    record = record + ";";

    if (thour < 10) {
      record = record + "0";
    }
    record = record + String(thour, DEC);
    record = record + ":";
    if (tmin < 10) {
      record = record + "0";
    }
    record = record + String(tmin, DEC);
    record = record + ":";
    if (tsec < 10) {
      record = record + "0";
    }
    record = record + String(tsec, DEC);
    record = record + ";";


    zjistiDHT22();
    sensors.requestTemperatures();

    Serial.print("Probe 01 temperature is:   ");
    Serial.print(myTemperature(Probe01, Correction1));
    Serial.println("°C");
    delay(170);
    Serial.print("t1=");
    Serial.println(myTemperature(Probe01, Correction1));
    delay(180);
    record = record + String(myTemperature(Probe01, Correction1));
    record = record + ";";

    Serial.print("Probe 02 temperature is:   ");
    Serial.print(myTemperature(Probe02, Correction2));
    Serial.println("°C");
    delay(170);
    Serial.print("t2=");
    Serial.println(myTemperature(Probe02, Correction2));
    delay(170);
    record = record + String(myTemperature(Probe02, Correction2));
    record = record + ";";

    Serial.print("Probe 03 temperature is:   ");
    Serial.print(myTemperature(Probe03, Correction3));
    Serial.println("°C");
    delay(170);
    Serial.print("t3=");
    Serial.println(myTemperature(Probe03, Correction3));
    delay(170);
    record = record + String(myTemperature(Probe03, Correction3));
    record = record + ";";

    Serial.print("Probe 04 temperature is:   ");
    Serial.print(myTemperature(Probe04, Correction4));
    Serial.println("°C");
    delay(170);
    Serial.print("t4=");
    Serial.println(myTemperature(Probe04, Correction4));
    delay(170);
    record = record + String(myTemperature(Probe04, Correction4));
    record = record + ";";

    record = record + String(temp);
    record = record + ";";
    record = record + String(humidity);
    record = record + ";";


    if (topeni < maxtemp && mintemp > -30) {
      digitalWrite(relePIN, HIGH);
      Serial.println("Heating");
      record = record + "Y";
    }
    else {
      digitalWrite(relePIN, LOW);
      Serial.println("Monitoring");
      record = record + "N";
    }

    //Serial.println(record);

    lastJob5s = millis();
  }

  //LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-LOOP-BLOCK-30s-
  if (millis() > (30000 + lastJob30s))
  {
    // kód vykonaný každých 30 vteřin (30000 ms)
    //AktualizujRTC();
    Serial.println("-------------------------------------");
    displayTime();
    Serial.println("-------------------------------------");

    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    myFile = SD.open("itrubec.csv", FILE_WRITE);

    // if the file opened okay, write to it:
    if (myFile) {
      Serial.print("Writing log itrubec.csv on SD card... ");
      myFile.println(record);
      // close the file:
      myFile.close();
      Serial.println("Done OK.");
    } else {
      // if the file didn't open, print an error:
      Serial.println("Error opening itrubec.csv");
    }

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
      Serial.print("Pripojeno k WiFi na IP adrese: ");
      Serial.println(WiFi.localIP().toString());
      AktualizujRTC();
    }

    lastJob1min = millis();
  }

  //LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-LOOP-BLOCK-
}
