/* Sử dụng thư viện */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "SPI.h"
#include <EEPROM.h>
#include <Wire.h>
#include "DS1307.h"

/* Cấu trúc thông số wifi */
typedef struct
{
  String ssid;
  String pass;
  uint8_t mode;
} WiFiConfigType;

/* Cấu trúc thông số hẹn giờ */
typedef struct
{
  uint8_t config;
  int hour;
  int minute;
} alarmConfigType;

/* Cấu trúc thông số thời gian */
typedef struct
{
  int hour;
  int minute;
  int second;
  int weak;
  int day;
  int month;
  int year;
} timeConfigType;

/* Cấu trúc thông số EEPROM */
typedef struct
{
  WiFiConfigType wifi;
  alarmConfigType alarm1;
  alarmConfigType alarm2;
  alarmConfigType alarm3;
} eepromConfigType;

/* Định nghĩa chân */
#define LATCH_PIN 15
#define NTC10K_PIN A0
#define SPEAKER_PIN 2
#define BUTTON_PIN 0

/* Thông số điểm truy cập AP */
#define APSSID_DEFAULT "Dong_Ho_Internet"
#define APPASS_DEFAULT "1234567890"

/* Định nghĩa địa chỉ lưu EEPROM */
#define EWSSID_ADDR 64     //64 byte
#define EWPASS_ADDR 128    //64 byte
#define EWMODE_ADDR 255    //1 byte
#define EACONFIG1_ADDR 256 //1 byte
#define EAHOUR1_ADDR 257   //2 byte
#define EAMINUTE1_ADDR 259 //2 byte
#define EACONFIG2_ADDR 261 //1 byte
#define EAHOUR2_ADDR 262   //2 byte
#define EAMINUTE2_ADDR 264 //2 byte
#define EACONFIG3_ADDR 266 //1 byte
#define EAHOUR3_ADDR 267   //2 byte
#define EAMINUTE3_ADDR 269 //2 byte

/* Các thông số của server */
#define ACONFIG_BIT7_COUNT 2
#define AHOUR_COUNT 24
#define AMINUTE_COUNT 60
#define BIT_SET 1
#define BIT_RESET 0

/* Thời điểm bắt đầu chuyển màn hình khác */
#define SECOND_START 24

/* Bảng mã LED 7 thanh bên trái và phải */
uint8_t ledHexNumberLeft[] = {0x05, 0x3F, 0x46, 0x16, 0x3C, 0x94, 0x84, 0x37, 0x04, 0x14};
uint8_t ledHexNumberRight[] = {0x05, 0xED, 0x46, 0xC4, 0xAC, 0x94, 0x14, 0xCD, 0x04, 0x84};
uint8_t ledHexWeekLeft[] = {0xC5, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
uint8_t ledHexWeekRight[] = {0x7C, 0x46, 0xC4, 0xAC, 0x94, 0x14, 0xCD};
uint8_t ledHexLoadingLeft[] = {0xF7, 0x7F, 0xBF, 0xDF, 0xEF, 0xFD};
uint8_t ledHexLoadingRight[] = {0xDF, 0xEF, 0xFD, 0xF7, 0x7F, 0xBF};

/* Bảng mã biểu tượng hiển thị */
uint8_t ledHexWifi[] = {0xED, 0xEB, 0xFD, 0xED};
uint8_t ledHexOffAlarm[] = {0xFF, 0xC8, 0x17, 0xFF};
uint8_t ledHexReset[] = {0xFF, 0xEA, 0x94, 0xFF};
uint8_t ledHexConfig[] = {0xFF, 0xCA, 0x2C, 0xFF};

/* Biến webserver */
ESP8266WebServer server(80);
String scanList, content, message;
int statusCode;

/* Biến wifi UDP (time NTP)*/
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, 25200);

/* Biến thông số wifi và hẹn giờ */
WiFiConfigType wifi;
alarmConfigType alarm1;
alarmConfigType alarm2;
alarmConfigType alarm3;

/* Biến nhiệt độ */
double temperature = 0.0;

/* Biến danh mục */
double menuTime = 0.0;
uint8_t menuSelect = 0;
uint8_t resetModule = 0;

/* Biến thời gian */
DS1307 timeRTC;
timeConfigType timeNow;
int timeNowSecondOld = 0;

/* Biến hiển thị */
uint16_t loadCounter = 0;
uint16_t spkCounter = 0;
uint16_t timeCounter = 0;
uint16_t fadeInCounter = 0;
uint16_t fadeOutCounter = 0;
uint8_t offSpeaker = 0;
uint8_t onFadeInTime = 0;
uint8_t onFadeOutTime = 0;
uint8_t onFadeInWeek = 0;
uint8_t onFadeOutWeek = 0;
uint8_t onFadeInDate = 0;
uint8_t onFadeOutDate = 0;
uint8_t onFadeInTemp = 0;
uint8_t onFadeOutTemp = 0;
uint32_t prevMillis1 = 0;
uint32_t prevMillis2 = 0;
uint32_t prevMillis3 = 0;
uint32_t prevMillis4 = 0;

/* Danh sách lựa chọn giờ, phút, giây server */
int alarmConfigBit7List[ACONFIG_BIT7_COUNT];
int alarmHourList[AHOUR_COUNT];
int alarmMinuteList[AMINUTE_COUNT];

/* Nguyên mẫu hàm */
void setLedHex(uint8_t, uint8_t, uint8_t, uint8_t);
void setFadeIn(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
void setFadeOut(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
void WifiConnect(void);
void WifiScan(void);
void WifiConfig(void);
void ClearEEPROMParam(void);
void WriteEEPROMParam(eepromConfigType, int);
void ReadEEPROMParam(eepromConfigType *);
void ButtonCheck(void);
double getTemperature(uint32_t, int, double);
double getResistence(double, uint32_t);
void printAlarmConfig(uint8_t);

/* Hàm khởi tạo */
void setup()
{
  wifi.mode = 1;
  wifi.ssid = "";
  wifi.pass = "";

  pinMode(LATCH_PIN, OUTPUT);
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(LATCH_PIN, HIGH);

  EEPROM.begin(512);
  delay(10);

  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println();
  Serial.println();

  timeRTC.begin();
  timeRTC.stopClock();

  SPI.begin();
  setLedHex(0xFF, 0xFF, 0xFF, 0xFF);

  eepromConfigType eepromConfigRead;
  ReadEEPROMParam(&eepromConfigRead);
  wifi.ssid = eepromConfigRead.wifi.ssid;
  wifi.pass = eepromConfigRead.wifi.pass;
  wifi.mode = eepromConfigRead.wifi.mode;
  alarm1.config = eepromConfigRead.alarm1.config;
  alarm1.hour = eepromConfigRead.alarm1.hour;
  alarm1.minute = eepromConfigRead.alarm1.minute;
  alarm2.config = eepromConfigRead.alarm2.config;
  alarm2.hour = eepromConfigRead.alarm2.hour;
  alarm2.minute = eepromConfigRead.alarm2.minute;
  alarm3.config = eepromConfigRead.alarm3.config;
  alarm3.hour = eepromConfigRead.alarm3.hour;
  alarm3.minute = eepromConfigRead.alarm3.minute;

  digitalWrite(SPEAKER_PIN, LOW);
  delay(300);
  digitalWrite(SPEAKER_PIN, HIGH);

  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.println("  DONG HO INTERNET V1.2");
  Serial.println("  VU PHAT          2020");
  Serial.println("  ===================================================");

  alarmConfigBit7List[0] = BIT_RESET;
  alarmConfigBit7List[1] = BIT_SET;
  for (int i = 0; i < AHOUR_COUNT; i++)
    alarmHourList[i] = i;
  for (int i = 0; i < AMINUTE_COUNT; i++)
    alarmMinuteList[i] = i;

  if (wifi.mode == 2)
  {
    eepromConfigType eepromConfigWrite;
    eepromConfigWrite.wifi.mode = 0;
    WriteEEPROMParam(eepromConfigWrite, 2);
    delay(2000);
    ESP.reset();
  }
  else if (wifi.mode == 1)
  {
    WiFi.mode(WIFI_AP);
    WifiConfig();
    eepromConfigType eepromConfigWrite;
    eepromConfigWrite.wifi.mode = 0;
    WriteEEPROMParam(eepromConfigWrite, 2);
  }
  else
  {
    WiFi.mode(WIFI_STA);
    WifiConnect();
  }
}

void loop()
{
  ButtonCheck();

  if (resetModule == 1)
  {
    Serial.println("> Module se tu khoi dong sau 5 giay nua!");
    delay(5000);
    ESP.reset();
  }

  if (wifi.mode == 1)
  {
    if (millis() - prevMillis4 >= 200)
    {
      prevMillis4 = millis();
      setLedHex(ledHexWifi[0], ledHexWifi[1], ledHexWifi[2], ledHexWifi[3]);
    }
    server.handleClient();
  }
  else if (wifi.mode == 0)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      ESP.reset();
    }
    else
    {
      timeRTC.getTime();
      timeNow.hour = timeRTC.hour;
      timeNow.minute = timeRTC.minute;
      timeNow.second = timeRTC.second;
      timeNow.weak = (timeRTC.dayOfWeek == 7 ? 0 : timeRTC.dayOfWeek);
      timeNow.day = timeRTC.dayOfMonth;
      timeNow.month = timeRTC.month;
      timeNow.year = timeRTC.year + 2000;

      if (timeNow.second != timeNowSecondOld)
      {
        if (timeNow.minute == 0 && timeNow.second == 0 && timeClient.update())
        {
          timeNow.hour = timeClient.getHours();
          timeNow.minute = timeClient.getMinutes();
          timeNow.second = timeClient.getSeconds();
          timeNow.weak = timeClient.getDay();
          timeClient.getDate(&timeNow.day, &timeNow.month, &timeNow.year);

          timeRTC.fillByYMD(timeNow.year, timeNow.month, timeNow.day);
          timeRTC.fillByHMS(timeNow.hour, timeNow.minute, timeNow.second);
          timeRTC.fillDayOfWeek(timeNow.weak == 0 ? 7 : timeNow.weak);
          timeRTC.setTime();
        }

        switch (timeNow.second)
        {
        case 0:
          offSpeaker = 0;
          break;

        case SECOND_START + 0:
          onFadeOutTime = 1;
          prevMillis1 = millis();
          break;

        case SECOND_START + 1:
          onFadeInWeek = 1;
          prevMillis1 = millis();
          break;

        case SECOND_START + 2:
        case SECOND_START + 3:
          setLedHex(0xFF, ledHexWeekLeft[timeNow.weak], ledHexWeekRight[timeNow.weak], 0xFF);
          break;

        case SECOND_START + 4:
          onFadeOutWeek = 1;
          prevMillis1 = millis();
          break;

        case SECOND_START + 5:
          onFadeInDate = 1;
          prevMillis1 = millis();
          break;

        case SECOND_START + 6:
        case SECOND_START + 7:
          setLedHex(ledHexNumberLeft[timeNow.day / 10], ledHexNumberLeft[timeNow.day % 10] & 0xFB, ledHexNumberRight[timeNow.month / 10], ledHexNumberRight[timeNow.month % 10]);
          break;

        case SECOND_START + 8:
          onFadeOutDate = 1;
          prevMillis1 = millis();
          break;

        case SECOND_START + 9:
          temperature = getTemperature(10000, 3950, getResistence((double)analogRead(NTC10K_PIN), 10000)) * 9.14;
          onFadeInTemp = 1;
          prevMillis1 = millis();
          break;

        case SECOND_START + 10:
        case SECOND_START + 11:
          setLedHex(ledHexNumberLeft[(uint16_t)temperature / 100], ledHexNumberLeft[(uint16_t)temperature / 10 % 10] & 0xFB, ledHexNumberRight[(uint16_t)temperature % 10], 0x13);
          break;

        case SECOND_START + 12:
          onFadeOutTemp = 1;
          prevMillis1 = millis();
          break;

        case SECOND_START + 13:
          onFadeInTime = 1;
          prevMillis1 = millis();
          break;

        default:
          if (timeNow.second % 2 != 0)
          {
            setLedHex(ledHexNumberLeft[timeNow.hour / 10], ledHexNumberLeft[timeNow.hour % 10] & 0xFB, ledHexNumberRight[timeNow.minute / 10] & 0xFB, ledHexNumberRight[timeNow.minute % 10]);
          }
          else
          {
            setLedHex(ledHexNumberLeft[timeNow.hour / 10], ledHexNumberLeft[timeNow.hour % 10], ledHexNumberRight[timeNow.minute / 10], ledHexNumberRight[timeNow.minute % 10]);
          }
          break;
        }

        timeNowSecondOld = timeNow.second;
      }

      if (millis() - prevMillis1 >= 200)
      {
        prevMillis1 = millis();

        if (onFadeInTime == 1)
        {
          setFadeIn(ledHexNumberLeft[timeNow.hour / 10], ledHexNumberLeft[timeNow.hour % 10], ledHexNumberRight[timeNow.minute / 10], ledHexNumberRight[timeNow.minute % 10], fadeInCounter);
          if (++fadeInCounter > 4)
          {
            fadeInCounter = 0;
            onFadeInTime = 0;
          }
        }

        if (onFadeInWeek == 1)
        {
          setFadeIn(0xFF, ledHexWeekLeft[timeNow.weak], ledHexWeekRight[timeNow.weak], 0xFF, fadeInCounter);
          if (++fadeInCounter > 4)
          {
            fadeInCounter = 0;
            onFadeInWeek = 0;
          }
        }

        if (onFadeInDate == 1)
        {
          setFadeIn(ledHexNumberLeft[timeNow.day / 10], ledHexNumberLeft[timeNow.day % 10] & 0xFB, ledHexNumberRight[timeNow.month / 10], ledHexNumberRight[timeNow.month % 10], fadeInCounter);
          if (++fadeInCounter > 4)
          {
            fadeInCounter = 0;
            onFadeInDate = 0;
          }
        }

        if (onFadeInTemp == 1)
        {
          setFadeIn(ledHexNumberLeft[(uint16_t)temperature / 100], ledHexNumberLeft[(uint16_t)temperature / 10 % 10] & 0xFB, ledHexNumberRight[(uint16_t)temperature % 10], 0x13, fadeInCounter);
          if (++fadeInCounter > 4)
          {
            fadeInCounter = 0;
            onFadeInTemp = 0;
          }
        }

        if (onFadeOutTime == 1)
        {
          setFadeOut(ledHexNumberLeft[timeNow.hour / 10], ledHexNumberLeft[timeNow.hour % 10], ledHexNumberRight[timeNow.minute / 10], ledHexNumberRight[timeNow.minute % 10], fadeOutCounter);
          if (++fadeOutCounter > 4)
          {
            fadeOutCounter = 0;
            onFadeOutTime = 0;
          }
        }

        if (onFadeOutWeek == 1)
        {
          setFadeOut(0xFF, ledHexWeekLeft[timeNow.weak], ledHexWeekRight[timeNow.weak], 0xFF, fadeOutCounter);
          if (++fadeOutCounter > 4)
          {
            fadeOutCounter = 0;
            onFadeOutWeek = 0;
          }
        }

        if (onFadeOutDate == 1)
        {
          setFadeOut(ledHexNumberLeft[timeNow.day / 10], ledHexNumberLeft[timeNow.day % 10] & 0xFB, ledHexNumberRight[timeNow.month / 10], ledHexNumberRight[timeNow.month % 10], fadeOutCounter);
          if (++fadeOutCounter > 4)
          {
            fadeOutCounter = 0;
            onFadeOutDate = 0;
          }
        }

        if (onFadeOutTemp == 1)
        {
          setFadeOut(ledHexNumberLeft[(uint16_t)temperature / 100], ledHexNumberLeft[(uint16_t)temperature / 10 % 10] & 0xFB, ledHexNumberRight[(uint16_t)temperature % 10], 0x13, fadeOutCounter);
          if (++fadeOutCounter > 4)
          {
            fadeOutCounter = 0;
            onFadeOutTemp = 0;
          }
        }
      }

      if (millis() - prevMillis3 >= 100)
      {
        prevMillis3 = millis();

        if (
            /* Alarm 1 */
            ((((alarm1.config >> 7) & 0x01) == BIT_SET && offSpeaker == 0) &&
             (((alarm1.config >> timeNow.weak) & 0x01) == BIT_SET) &&
             (timeNow.hour == alarm1.hour && timeNow.minute == alarm1.minute)) ||

            /* Alarm 2 */
            ((((alarm2.config >> 7) & 0x01) == BIT_SET && offSpeaker == 0) &&
             (((alarm2.config >> timeNow.weak) & 0x01) == BIT_SET) &&
             (timeNow.hour == alarm2.hour && timeNow.minute == alarm2.minute)) ||

            /* Alarm 3 */
            ((((alarm3.config >> 7) & 0x01) == BIT_SET && offSpeaker == 0) &&
             (((alarm3.config >> timeNow.weak) & 0x01) == BIT_SET) &&
             (timeNow.hour == alarm3.hour && timeNow.minute == alarm3.minute)))
        {
          switch (spkCounter)
          {
          case 0:
            digitalWrite(SPEAKER_PIN, HIGH);
            break;
          case 1:
            digitalWrite(SPEAKER_PIN, LOW);
            break;
          case 2:
            digitalWrite(SPEAKER_PIN, HIGH);
            break;
          case 3:
            digitalWrite(SPEAKER_PIN, LOW);
            break;
          case 4:
            digitalWrite(SPEAKER_PIN, HIGH);
            break;
          case 5:
            digitalWrite(SPEAKER_PIN, LOW);
            break;
          case 6:
            digitalWrite(SPEAKER_PIN, HIGH);
            break;
          }

          if (++spkCounter > 9)
            spkCounter = 0;
        }
        else
        {
          digitalWrite(SPEAKER_PIN, HIGH);
        }
      }
    }
  }
}

/* Hàm gửi mã LED */
void setLedHex(uint8_t hex1, uint8_t hex2, uint8_t hex3, uint8_t hex4)
{
  uint8_t buff[4] = {hex3, hex4, hex2, hex1};
  digitalWrite(LATCH_PIN, LOW);
  SPI.transfer(buff, sizeof(buff));
  digitalWrite(LATCH_PIN, HIGH);
}

/* Hàm hiệu ứng đi vào trong */
void setFadeIn(uint8_t hex1, uint8_t hex2, uint8_t hex3, uint8_t hex4, uint8_t step)
{
  switch (step)
  {
  case 0:
  case 1:
    setLedHex(0xFF, 0xFF, 0xFF, 0xFF);
    break;

  case 2:
    setLedHex(hex2, 0xFF, 0xFF, hex3);
    break;

  case 3:
  case 4:
    setLedHex(hex1, hex2, hex3, hex4);
    break;
  }
}

/* Hàm hiệu ứng đi ra ngoài */
void setFadeOut(uint8_t hex1, uint8_t hex2, uint8_t hex3, uint8_t hex4, uint8_t step)
{
  switch (step)
  {
  case 0:
  case 1:
    setLedHex(hex1, hex2, hex3, hex4);
    break;

  case 2:
    setLedHex(hex2, 0xFF, 0xFF, hex3);
    break;

  case 3:
  case 4:
    setLedHex(0xFF, 0xFF, 0xFF, 0xFF);
    break;
  }
}

/* Hàm kết nối Wifi */
void WifiConnect()
{
  Serial.print("> Dang ket noi toi ");
  Serial.println(wifi.ssid.c_str());
  WiFi.begin(wifi.ssid.c_str(), wifi.pass.c_str());
  uint32_t wifiCounter = 0;

  while (WiFi.status() != WL_CONNECTED)
  {
    ButtonCheck();
    delay(100);

    if (++loadCounter > 5)
      loadCounter = 0;
    setLedHex(ledHexLoadingLeft[loadCounter], ledHexLoadingLeft[loadCounter], ledHexLoadingRight[loadCounter], ledHexLoadingRight[loadCounter]);

    if (wifiCounter++ >= 300)
    {
      ESP.reset();
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("  ");
    Serial.print(WiFi.SSID());
    Serial.println(" da duoc ket noi!");
    Serial.print("  Dia chi IP: ");
    Serial.println(WiFi.localIP());

    timeClient.begin();
    if (timeClient.update())
    {
      timeNow.hour = timeClient.getHours();
      timeNow.minute = timeClient.getMinutes();
      timeNow.second = timeClient.getSeconds();
      timeNow.weak = timeClient.getDay();
      timeClient.getDate(&timeNow.day, &timeNow.month, &timeNow.year);

      char str[32];
      Serial.print("  Thời gian NTP: ");
      sprintf(str, "%02d:%02d:%02d", timeNow.hour, timeNow.minute, timeNow.second);
      Serial.println(str);
      Serial.print("  Ngày NTP: ");
      const char *wk[7] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
      Serial.print(wk[timeNow.weak]);
      Serial.print(",");
      sprintf(str, "%02d/%02d/%04d", timeNow.day, timeNow.month, timeNow.year);
      Serial.println(str);

      timeRTC.fillByYMD(timeNow.year, timeNow.month, timeNow.day);
      timeRTC.fillByHMS(timeNow.hour, timeNow.minute, timeNow.second);
      timeRTC.fillDayOfWeek(timeNow.weak == 0 ? 7 : timeNow.weak);
      timeRTC.setTime();
    }

    digitalWrite(SPEAKER_PIN, LOW);
    delay(300);
    digitalWrite(SPEAKER_PIN, HIGH);
  }

  temperature = getTemperature(10000, 3950, getResistence((double)analogRead(NTC10K_PIN), 10000)) * 9.14;
  Serial.print("  Nhiet do: ");
  Serial.println((double)temperature / 10, 1);
}

/* Hàm quét Wifi */
void WifiScan(void)
{
  int n = WiFi.scanNetworks();
  scanList = "<ul id='wifi-list'>";
  for (int i = 0; i < n; ++i)
  {
    scanList += "<li><a href='#' onclick='getSSID(this);return false;'>";
    scanList += WiFi.SSID(i);
    scanList += "</a> (<span>";
    scanList += WiFi.RSSI(i);
    scanList += "</span>)";
    scanList += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    scanList += "</li>";
  }
  scanList += "<li><a href='/'>Quét lại</a>";
  scanList += "</ul>";
}

/* Hàm trang cấu hình Wifi */
void WifiConfig(void)
{
  setLedHex(ledHexWifi[0], ledHexWifi[1], ledHexWifi[2], ledHexWifi[3]);
  Serial.println("> Che do diem truy cap");
  Serial.println("  IP: " + WiFi.softAPIP().toString());
  Serial.println("  Ten: " + String(APSSID_DEFAULT));
  Serial.println("  MK: " + String(APPASS_DEFAULT));
  WiFi.softAP(APSSID_DEFAULT, APPASS_DEFAULT);
  server.on("/", []() {
    WifiScan();
    content = "<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0 Transitional//EN' 'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'>";
    content += "<html xmlns='http://www.w3.org/1999/xhtml'>";
    content += "<head><meta http-equiv='Content-Type' content='text/html;charset=utf-8'/><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>" + String(APSSID_DEFAULT) + "</title></head>";
    content += "<body onload='sortWifiList()'><div class='container'>";

    /* ___Header____ */
    content += "<div class='header-wrapper'><div class='header'>" + String(APSSID_DEFAULT) + "</div></div>";

    /* ___Content___ */
    content += "<div class='content'>";

    /* Wifi scan list */
    content += "<div class='wifi-scan'>" + scanList + "</div>";

    /* Wifi config */
    content += "<div class='wifi-config'><form method='get' action='setting'>";
    content += "<div class='frame-title'><b>Tên WiFi:</b></div><div class='frame-content'><input id='wifi-ssid' type='text' name='wssid' value='" + String(wifi.ssid.c_str()) + "' length=64></div>";
    content += "<div class='frame-title'><b>Mật khẩu WiFi:</b></div><div class='frame-content'><input type='text' name='wpass' value='" + String(wifi.pass.c_str()) + "' length=64></div>";

    /* Alarm1 config */
    content += "<div class='frame-title'><b>Báo thức 1:</b></div><div class='frame-content'>";
    content += "<div class='frame-content-item'><select id='aconfig1bit7' onclick='selectChange(this.id)'>";
    for (int i = 0; i < ACONFIG_BIT7_COUNT; i++)
      content += "<option value='" + String(alarmConfigBit7List[i]) + "' " + String((((alarm1.config >> 7) & 0x01) == alarmConfigBit7List[i]) ? "selected" : "") + ">" + String((alarmConfigBit7List[i] == BIT_SET) ? "Bật" : "Tắt") + "</option>";
    content += "</select></div>, ";
    content += "<div class='frame-content-item'><select name='ahour1'>";
    for (int i = 0; i < AHOUR_COUNT; i++)
      content += "<option value='" + String(alarmHourList[i]) + "' " + String((alarm1.hour == alarmHourList[i]) ? "selected" : "") + ">" + alarmHourList[i] + "</option>";
    content += "</select></div> : ";
    content += "<div class='frame-content-item'><select name='aminute1'>";
    for (int i = 0; i < AMINUTE_COUNT; i++)
      content += "<option value='" + String(alarmMinuteList[i]) + "' " + String((alarm1.minute == alarmMinuteList[i]) ? "selected" : "") + ">" + alarmMinuteList[i] + "</option>";
    content += "</select></div></div>";

    /* Repeat 1 config */
    content += "<div class='frame-title'><b>Lặp Lại báo thức 1:</b></div><div class='frame-content'>";
    content += "<input id='aconfig1bit0' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm1.config >> 0) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig1bit0'>Chủ nhật</label><br />";
    content += "<input id='aconfig1bit1' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm1.config >> 1) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig1bit1'>Thứ hai</label><br />";
    content += "<input id='aconfig1bit2' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm1.config >> 2) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig1bit2'>Thứ ba</label><br />";
    content += "<input id='aconfig1bit3' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm1.config >> 3) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig1bit3'>Thứ tư</label><br />";
    content += "<input id='aconfig1bit4' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm1.config >> 4) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig1bit4'>Thứ năm</label><br />";
    content += "<input id='aconfig1bit5' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm1.config >> 5) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig1bit5'>Thứ sáu</label><br />";
    content += "<input id='aconfig1bit6' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm1.config >> 6) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig1bit6'>Thứ bảy</label>";
    content += "<br /><input id='aconfig1' class='hidden' type='text' name='aconfig1' value='" + String(alarm1.config) + "'>";
    content += "</div>";

    /* Alarm2 config */
    content += "<div class='frame-title'><b>Báo thức 2:</b></div><div class='frame-content'>";
    content += "<div class='frame-content-item'><select id='aconfig2bit7' onclick='selectChange(this.id)'>";
    for (int i = 0; i < ACONFIG_BIT7_COUNT; i++)
      content += "<option value='" + String(alarmConfigBit7List[i]) + "' " + String((((alarm2.config >> 7) & 0x01) == alarmConfigBit7List[i]) ? "selected" : "") + ">" + String((alarmConfigBit7List[i] == BIT_SET) ? "Bật" : "Tắt") + "</option>";
    content += "</select></div>, ";
    content += "<div class='frame-content-item'><select name='ahour2'>";
    for (int i = 0; i < AHOUR_COUNT; i++)
      content += "<option value='" + String(alarmHourList[i]) + "' " + String((alarm2.hour == alarmHourList[i]) ? "selected" : "") + ">" + alarmHourList[i] + "</option>";
    content += "</select></div> : ";
    content += "<div class='frame-content-item'><select name='aminute2'>";
    for (int i = 0; i < AMINUTE_COUNT; i++)
      content += "<option value='" + String(alarmMinuteList[i]) + "' " + String((alarm2.minute == alarmMinuteList[i]) ? "selected" : "") + ">" + alarmMinuteList[i] + "</option>";
    content += "</select></div></div>";

    /* Repeat 2 config */
    content += "<div class='frame-title'><b>Lặp Lại báo thức 2:</b></div><div class='frame-content'>";
    content += "<input id='aconfig2bit0' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm2.config >> 0) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig2bit0'>Chủ nhật</label><br />";
    content += "<input id='aconfig2bit1' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm2.config >> 1) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig2bit1'>Thứ hai</label><br />";
    content += "<input id='aconfig2bit2' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm2.config >> 2) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig2bit2'>Thứ ba</label><br />";
    content += "<input id='aconfig2bit3' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm2.config >> 3) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig2bit3'>Thứ tư</label><br />";
    content += "<input id='aconfig2bit4' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm2.config >> 4) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig2bit4'>Thứ năm</label><br />";
    content += "<input id='aconfig2bit5' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm2.config >> 5) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig2bit5'>Thứ sáu</label><br />";
    content += "<input id='aconfig2bit6' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm2.config >> 6) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig2bit6'>Thứ bảy</label>";
    content += "<br /><input id='aconfig2' class='hidden' type='text' name='aconfig2' value='" + String(alarm2.config) + "'>";
    content += "</div>";

    /* Alarm3 config */
    content += "<div class='frame-title'><b>Báo thức 3:</b></div><div class='frame-content'>";
    content += "<div class='frame-content-item'><select id='aconfig3bit7' onclick='selectChange(this.id)'>";
    for (int i = 0; i < ACONFIG_BIT7_COUNT; i++)
      content += "<option value='" + String(alarmConfigBit7List[i]) + "' " + String((((alarm3.config >> 7) & 0x01) == alarmConfigBit7List[i]) ? "selected" : "") + ">" + String((alarmConfigBit7List[i] == BIT_SET) ? "Bật" : "Tắt") + "</option>";
    content += "</select></div>, ";
    content += "<div class='frame-content-item'><select name='ahour3'>";
    for (int i = 0; i < AHOUR_COUNT; i++)
      content += "<option value='" + String(alarmHourList[i]) + "' " + String((alarm3.hour == alarmHourList[i]) ? "selected" : "") + ">" + alarmHourList[i] + "</option>";
    content += "</select></div> : ";
    content += "<div class='frame-content-item'><select name='aminute3'>";
    for (int i = 0; i < AMINUTE_COUNT; i++)
      content += "<option value='" + String(alarmMinuteList[i]) + "' " + String((alarm3.minute == alarmMinuteList[i]) ? "selected" : "") + ">" + alarmMinuteList[i] + "</option>";
    content += "</select></div></div>";

    /* Repeat 2 config */
    content += "<div class='frame-title'><b>Lặp Lại báo thức 2:</b></div><div class='frame-content'>";
    content += "<input id='aconfig3bit0' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm3.config >> 0) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig3bit0'>Chủ nhật</label><br />";
    content += "<input id='aconfig3bit1' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm3.config >> 1) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig3bit1'>Thứ hai</label><br />";
    content += "<input id='aconfig3bit2' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm3.config >> 2) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig3bit2'>Thứ ba</label><br />";
    content += "<input id='aconfig3bit3' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm3.config >> 3) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig3bit3'>Thứ tư</label><br />";
    content += "<input id='aconfig3bit4' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm3.config >> 4) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig3bit4'>Thứ năm</label><br />";
    content += "<input id='aconfig3bit5' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm3.config >> 5) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig3bit5'>Thứ sáu</label><br />";
    content += "<input id='aconfig3bit6' type='checkbox' onclick='checkboxChange(this.id)' " + String((((alarm3.config >> 6) & 0x01) == BIT_SET) ? "checked" : "") + "><label for='aconfig3bit6'>Thứ bảy</label>";
    content += "<br /><input id='aconfig3' class='hidden' type='text' name='aconfig3' value='" + String(alarm3.config) + "'>";
    content += "</div>";

    /* Apply button */
    content += "<input type='submit' value='Lưu thông số'/><br /></form>";
    content += "</div></div></div>";

    /* Style */
    content += "<style>";
    content += "body{font-family:arial,san-serif;font-size:14px;color:rgba(0,0,0,.87);background:#f5f5f5;}.container{width:400px; margin-left:auto; margin-right:auto;border:1px solid #e0e0e0;background: #fff;}.header-wrapper{background:rgba(0,0,0,.87);color:#fff;font-weight:bold;font-size:18px;}.header{padding:10px 5px;text-align:center;}.content{padding:15px;}@media screen and (max-width:600px){.container{width:100%;}}";
    content += ".wifi-scan{padding:0 0 20px;}select{width:100%;height:25px;margin:3px 0 9px;border:1px solid #e0e0e0;font-size:15px}input[type=text],input[type=number]{width:100%;height:21px;margin:3px 0 9px;border:1px solid #e0e0e0;font-size:15px}input[type=submit]{width:100%;padding:5px 10px;margin-top:10px;border:1px solid #e0e0e0;background:#f0f0f0;}ul{list-style-type:disclosure-closed;margin:0;padding:0 0 0 30px;}";
    content += ".frame-title{width:100%;display:block;}.frame-content{width:100%;display:block;margin:3px 0 0;}.frame-content-item{width:31%;display:inline-block;}.frame-content-item input{text-align:right;}.hidden{display:none}";
    content += "</style>";

    /* Script */
    content += "<script type='text/javascript'>";
    content += "var getSSID=function(e){document.getElementById('wifi-ssid').value=e.text};";
    content += "function sortWifiList() {var list,i,switching,b,shouldSwitch;list=document.getElementById('wifi-list');switching=!0;while(switching){switching=!1;b=list.getElementsByTagName('span');for(i=0;i<(b.length-1);i++){shouldSwitch=!1;if(Number(b[i].innerHTML)<Number(b[i+1].innerHTML)){shouldSwitch=!0;break}}if(shouldSwitch){b[i].parentNode.parentNode.insertBefore(b[i+1].parentNode,b[i].parentNode);switching=!0}}}";
    content += "var aconfig=[" + String(alarm1.config) + "," + String(alarm2.config) + "," + String(alarm3.config) + "];function selectChange(e){var n=document.getElementById(e);'1'==n.value?aconfig[parseInt(e[7])-1]|=1<<7:'0'==n.value&&(aconfig[parseInt(e[7])-1]&=~(1<<7)),document.getElementById('aconfig1').value=aconfig[0].toString(),document.getElementById('aconfig2').value=aconfig[1].toString(),document.getElementById('aconfig3').value=aconfig[2].toString()}function checkboxChange(e){document.getElementById(e).checked?aconfig[parseInt(e[7])-1]|=1<<parseInt(e[11]):aconfig[parseInt(e[7])-1]&=~(1<<parseInt(e[11])),document.getElementById('aconfig1').value=aconfig[0].toString(),document.getElementById('aconfig2').value=aconfig[1].toString(),document.getElementById('aconfig3').value=aconfig[2].toString()}";
    content += "</script>";

    content += "</body></html>";
    server.send(200, "text/html", content);
  });

  server.on("/setting", []() {
    String wifiSSID = server.arg("wssid");
    String wifiPass = server.arg("wpass");
    String alarmConfig1 = server.arg("aconfig1");
    String alarmHour1 = server.arg("ahour1");
    String alarmMinute1 = server.arg("aminute1");
    String alarmConfig2 = server.arg("aconfig2");
    String alarmHour2 = server.arg("ahour2");
    String alarmMinute2 = server.arg("aminute2");
    String alarmConfig3 = server.arg("aconfig3");
    String alarmHour3 = server.arg("ahour3");
    String alarmMinute3 = server.arg("aminute3");

    if (wifiSSID.length() > 0 && wifiPass.length() > 0 &&
        alarmConfig1.length() > 0 && alarmHour1.length() > 0 && alarmMinute1.length() > 0 &&
        alarmConfig2.length() > 0 && alarmHour2.length() > 0 && alarmMinute2.length() > 0 &&
        alarmConfig3.length() > 0 && alarmHour3.length() > 0 && alarmMinute3.length() > 0)
    {
      ClearEEPROMParam();
      eepromConfigType eepromConfigWrite;
      eepromConfigWrite.wifi.ssid = wifiSSID;
      eepromConfigWrite.wifi.pass = wifiPass;
      eepromConfigWrite.wifi.mode = 2;
      eepromConfigWrite.alarm1.config = (uint8_t)alarmConfig1.toInt();
      eepromConfigWrite.alarm1.hour = alarmHour1.toInt();
      eepromConfigWrite.alarm1.minute = alarmMinute1.toInt();
      eepromConfigWrite.alarm2.config = (uint8_t)alarmConfig2.toInt();
      eepromConfigWrite.alarm2.hour = alarmHour2.toInt();
      eepromConfigWrite.alarm2.minute = alarmMinute2.toInt();
      eepromConfigWrite.alarm3.config = (uint8_t)alarmConfig3.toInt();
      eepromConfigWrite.alarm3.hour = alarmHour3.toInt();
      eepromConfigWrite.alarm3.minute = alarmMinute3.toInt();

      WriteEEPROMParam(eepromConfigWrite, 1);
      message = "Cấu hình thành công! Đã lưu các thông số!<br /><br />";
      message += "<b>Module sẽ tự khởi động lại sau 5 giây nữa.</b><br />";
      statusCode = 200;
      resetModule = 1;
    }
    else
    {
      Serial.println();
      Serial.println("> Loi 404! Khong tim thay trang!");
      message = "Lỗi 404! Không tìm thấy trang!<br /><br />";
      message += "<b>Module sẽ tự khởi động lại sau 5 giây nữa.</b><br />";
      statusCode = 404;
      resetModule = 1;
    }

    content = "<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0 Transitional//EN' 'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'>";
    content += "<html xmlns='http://www.w3.org/1999/xhtml'>";
    content += "<head><meta http-equiv='Content-Type' content='text/html;charset=utf-8'/><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>" + String(APSSID_DEFAULT) + "</title></head>";
    content += "<body><div class='container'>";
    content += "<div class='header-wrapper'><div class='header'>" + String(APSSID_DEFAULT) + "</div></div>";
    content += "<div class='content'><div class='message'>";
    content += message;
    content += "</div></div></div>";
    content += "<style>";
    content += "body{font-family:arial,san-serif;font-size:14px;color:rgba(0,0,0,.87);background:#f5f5f5;}.container{width:400px; margin-left:auto; margin-right:auto;border:1px solid #e0e0e0;background: #fff;}.header-wrapper{background:rgba(0,0,0,.87);color:#fff;font-weight:bold;font-size:18px;}.header{padding:10px 5px;text-align:center;}.content{padding:15px;}@media screen and (max-width: 600px) {.container {width: 100%;}}";
    content += "</style>";
    content += "</body></html>";
    server.send(statusCode, "text/html", content);
  });

  server.begin();
}

/* Hàm xóa EEPROM */
void ClearEEPROMParam(void)
{
  EEPROM.begin(512);
  delay(10);

  Serial.println("> Xoa thong so EEPROM!");
  for (unsigned int i = EWSSID_ADDR; i < EWSSID_ADDR + 64; ++i)
  {
    EEPROM.write(i, 0);
  }

  for (unsigned int i = EWPASS_ADDR; i < EWPASS_ADDR + 64; ++i)
  {
    EEPROM.write(i, 0);
  }

  EEPROM.write(EWMODE_ADDR, 0);

  EEPROM.write(EACONFIG1_ADDR, 0);
  EEPROM.write(EAHOUR1_ADDR, 0);
  EEPROM.write(EAHOUR1_ADDR + 1, 0);
  EEPROM.write(EAMINUTE1_ADDR, 0);
  EEPROM.write(EAMINUTE1_ADDR + 1, 0);

  EEPROM.write(EACONFIG2_ADDR, 0);
  EEPROM.write(EAHOUR2_ADDR, 0);
  EEPROM.write(EAHOUR2_ADDR + 1, 0);
  EEPROM.write(EAMINUTE2_ADDR, 0);
  EEPROM.write(EAMINUTE2_ADDR + 1, 0);

  EEPROM.write(EACONFIG3_ADDR, 0);
  EEPROM.write(EAHOUR3_ADDR, 0);
  EEPROM.write(EAHOUR3_ADDR + 1, 0);
  EEPROM.write(EAMINUTE3_ADDR, 0);
  EEPROM.write(EAMINUTE3_ADDR + 1, 0);

  EEPROM.commit();
}

/* Hàm Ghi EEPROM */
void WriteEEPROMParam(eepromConfigType eepromConfig, int writeMode)
{
  EEPROM.begin(512);
  delay(10);

  Serial.println("> Ghi EEPROM:");
  switch (writeMode)
  {
  case 1:
    Serial.print("  Ten WiFi: ");
    for (unsigned int i = 0; i < eepromConfig.wifi.ssid.length(); ++i)
    {
      EEPROM.write(EWSSID_ADDR + i, eepromConfig.wifi.ssid[i]);
      Serial.print(eepromConfig.wifi.ssid[i]);
    }
    Serial.println();

    Serial.print("  Mat khau WiFi: ");
    for (unsigned int i = 0; i < eepromConfig.wifi.pass.length(); ++i)
    {
      EEPROM.write(EWPASS_ADDR + i, eepromConfig.wifi.pass[i]);
      Serial.print(eepromConfig.wifi.pass[i]);
    }

    Serial.println();
    Serial.print("  Che do cau hinh: ");
    EEPROM.write(EWMODE_ADDR, eepromConfig.wifi.mode);
    Serial.println(eepromConfig.wifi.mode);

    Serial.print("  Bao thuc 1: ");
    EEPROM.write(EACONFIG1_ADDR, eepromConfig.alarm1.config & 0xFF);
    Serial.print(eepromConfig.alarm1.config ? "Bat" : "Tat");
    Serial.print(", ");
    EEPROM.write(EAHOUR1_ADDR, (eepromConfig.alarm1.hour >> 8) & 0xFF);
    EEPROM.write(EAHOUR1_ADDR + 1, eepromConfig.alarm1.hour & 0xFF);
    EEPROM.write(EAMINUTE1_ADDR, (eepromConfig.alarm1.minute >> 8) & 0xFF);
    EEPROM.write(EAMINUTE1_ADDR + 1, eepromConfig.alarm1.minute & 0xFF);
    char str1[12];
    sprintf(str1, "%02d:%02d", eepromConfig.alarm1.hour, eepromConfig.alarm1.minute);
    Serial.println(str1);

    Serial.print("  Bao thuc 2: ");
    EEPROM.write(EACONFIG2_ADDR, eepromConfig.alarm2.config & 0xFF);
    Serial.print(eepromConfig.alarm2.config ? "Bat" : "Tat");
    Serial.print(", ");
    EEPROM.write(EAHOUR2_ADDR, (eepromConfig.alarm2.hour >> 8) & 0xFF);
    EEPROM.write(EAHOUR2_ADDR + 1, eepromConfig.alarm2.hour & 0xFF);
    EEPROM.write(EAMINUTE2_ADDR, (eepromConfig.alarm2.minute >> 8) & 0xFF);
    EEPROM.write(EAMINUTE2_ADDR + 1, eepromConfig.alarm2.minute & 0xFF);
    sprintf(str1, "%02d:%02d", eepromConfig.alarm2.hour, eepromConfig.alarm2.minute);
    Serial.println(str1);

    Serial.print("  Bao thuc 3: ");
    EEPROM.write(EACONFIG3_ADDR, eepromConfig.alarm3.config & 0xFF);
    Serial.print(eepromConfig.alarm3.config ? "Bat" : "Tat");
    Serial.print(", ");
    EEPROM.write(EAHOUR3_ADDR, (eepromConfig.alarm3.hour >> 8) & 0xFF);
    EEPROM.write(EAHOUR3_ADDR + 1, eepromConfig.alarm3.hour & 0xFF);
    EEPROM.write(EAMINUTE3_ADDR, (eepromConfig.alarm3.minute >> 8) & 0xFF);
    EEPROM.write(EAMINUTE3_ADDR + 1, eepromConfig.alarm3.minute & 0xFF);
    sprintf(str1, "%02d:%02d", eepromConfig.alarm3.hour, eepromConfig.alarm3.minute);
    Serial.println(str1);
    break;

  case 2:
    Serial.print("  Che do cau hinh: ");
    EEPROM.write(EWMODE_ADDR, eepromConfig.wifi.mode);
    Serial.println(eepromConfig.wifi.mode);
    break;
  }

  EEPROM.commit();
}

/* Hàm đọc EEPROM */
void ReadEEPROMParam(eepromConfigType *eepromConfig)
{
  EEPROM.begin(512);
  delay(10);

  Serial.println("> Doc EEPROM:");

  Serial.print("  Ten WiFi: ");
  eepromConfig->wifi.ssid = "";
  for (unsigned int i = EWSSID_ADDR; i < EWSSID_ADDR + 64; ++i)
  {
    if (!isnan(EEPROM.read(i)))
      eepromConfig->wifi.ssid += char(EEPROM.read(i));
  }
  Serial.println(eepromConfig->wifi.ssid.c_str());

  Serial.print("  Mat khau WiFi: ");
  eepromConfig->wifi.pass = "";
  for (unsigned int i = EWPASS_ADDR; i < EWPASS_ADDR + 64; ++i)
  {
    if (!isnan(EEPROM.read(i)))
      eepromConfig->wifi.pass += char(EEPROM.read(i));
  }
  Serial.println(eepromConfig->wifi.pass.c_str());

  Serial.print("  Che do cau hinh: ");
  if (isnan(EEPROM.read(EWMODE_ADDR)))
    eepromConfig->wifi.mode = 1;
  else
    eepromConfig->wifi.mode = EEPROM.read(EWMODE_ADDR);
  Serial.println(eepromConfig->wifi.mode);

  Serial.print("  Bao thuc 1: ");
  if (isnan(EEPROM.read(EACONFIG1_ADDR)))
  {
    eepromConfig->alarm1.config = 0;
  }
  else
  {
    eepromConfig->alarm1.config = EEPROM.read(EACONFIG1_ADDR);
  }
  Serial.print(((((eepromConfig->alarm1.config >> 7) & 0x01) == BIT_SET) ? "Bat" : "Tat"));

  Serial.print(", ");
  if (isnan(EEPROM.read(EAHOUR1_ADDR)) || isnan(EEPROM.read(EAHOUR1_ADDR + 1)))
  {
    eepromConfig->alarm1.hour = 7;
  }
  else
  {
    eepromConfig->alarm1.hour = EEPROM.read(EAHOUR1_ADDR);
    eepromConfig->alarm1.hour <<= 8;
    eepromConfig->alarm1.hour |= EEPROM.read(EAHOUR1_ADDR + 1);
  }

  if (isnan(EEPROM.read(EAMINUTE1_ADDR)) || isnan(EEPROM.read(EAMINUTE1_ADDR + 1)))
  {
    eepromConfig->alarm1.minute = 0;
  }
  else
  {
    eepromConfig->alarm1.minute = EEPROM.read(EAMINUTE1_ADDR);
    eepromConfig->alarm1.minute <<= 8;
    eepromConfig->alarm1.minute |= EEPROM.read(EAMINUTE1_ADDR + 1);
  }

  char str[12];
  sprintf(str, "%02d:%02d", eepromConfig->alarm1.hour, eepromConfig->alarm1.minute);
  Serial.print(str);
  Serial.print(", lap lai ");
  printAlarmConfig(eepromConfig->alarm1.config);
  Serial.println();

  Serial.print("  Bao thuc 2: ");
  if (isnan(EEPROM.read(EACONFIG2_ADDR)))
  {
    eepromConfig->alarm2.config = 0;
  }
  else
  {
    eepromConfig->alarm2.config = EEPROM.read(EACONFIG2_ADDR);
  }
  Serial.print(((((eepromConfig->alarm2.config >> 7) & 0x01) == BIT_SET) ? "Bat" : "Tat"));

  Serial.print(", ");
  if (isnan(EEPROM.read(EAHOUR2_ADDR)) || isnan(EEPROM.read(EAHOUR2_ADDR + 1)))
  {
    eepromConfig->alarm2.hour = 7;
  }
  else
  {
    eepromConfig->alarm2.hour = EEPROM.read(EAHOUR2_ADDR);
    eepromConfig->alarm2.hour <<= 8;
    eepromConfig->alarm2.hour |= EEPROM.read(EAHOUR2_ADDR + 1);
  }

  if (isnan(EEPROM.read(EAMINUTE2_ADDR)) || isnan(EEPROM.read(EAMINUTE2_ADDR + 1)))
  {
    eepromConfig->alarm2.minute = 30;
  }
  else
  {
    eepromConfig->alarm2.minute = EEPROM.read(EAMINUTE2_ADDR);
    eepromConfig->alarm2.minute <<= 8;
    eepromConfig->alarm2.minute |= EEPROM.read(EAMINUTE2_ADDR + 1);
  }

  sprintf(str, "%02d:%02d", eepromConfig->alarm2.hour, eepromConfig->alarm2.minute);
  Serial.print(str);
  Serial.print(", lap lai ");
  printAlarmConfig(eepromConfig->alarm2.config);
  Serial.println();

  Serial.print("  Bao thuc 3: ");
  if (isnan(EEPROM.read(EACONFIG3_ADDR)))
  {
    eepromConfig->alarm3.config = 0;
  }
  else
  {
    eepromConfig->alarm3.config = EEPROM.read(EACONFIG3_ADDR);
  }
  Serial.print(((((eepromConfig->alarm3.config >> 7) & 0x01) == BIT_SET) ? "Bat" : "Tat"));

  Serial.print(", ");
  if (isnan(EEPROM.read(EAHOUR3_ADDR)) || isnan(EEPROM.read(EAHOUR3_ADDR + 1)))
  {
    eepromConfig->alarm3.hour = 8;
  }
  else
  {
    eepromConfig->alarm3.hour = EEPROM.read(EAHOUR3_ADDR);
    eepromConfig->alarm3.hour <<= 8;
    eepromConfig->alarm3.hour |= EEPROM.read(EAHOUR3_ADDR + 1);
  }

  if (isnan(EEPROM.read(EAMINUTE3_ADDR)) || isnan(EEPROM.read(EAMINUTE3_ADDR + 1)))
  {
    eepromConfig->alarm3.minute = 0;
  }
  else
  {
    eepromConfig->alarm3.minute = EEPROM.read(EAMINUTE3_ADDR);
    eepromConfig->alarm3.minute <<= 8;
    eepromConfig->alarm3.minute |= EEPROM.read(EAMINUTE3_ADDR + 1);
  }

  sprintf(str, "%02d:%02d", eepromConfig->alarm3.hour, eepromConfig->alarm3.minute);
  Serial.print(str);
  Serial.print(", lap lai ");
  printAlarmConfig(eepromConfig->alarm3.config);
  Serial.println();
}

/* Hàm kiểm tra nút nhấn */
void ButtonCheck(void)
{
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    menuTime = 0.0;
    do
    {
      delay(1);
      menuTime += 1.0;

      if (menuTime >= 9500.0)
      {
        menuTime = 0.0;
        menuSelect = 0;
        if (menuTime == 9500.0)
        {
          setLedHex(ledHexOffAlarm[0], ledHexOffAlarm[1], ledHexOffAlarm[2], ledHexOffAlarm[3]);
        }
      }
      else if (menuTime >= 6500.0)
      {
        menuSelect = 3;
        if (menuTime == 6500.0)
        {
          setLedHex(ledHexConfig[0], ledHexConfig[1], ledHexConfig[2], ledHexConfig[3]);
          digitalWrite(SPEAKER_PIN, LOW);
          delay(80);
          digitalWrite(SPEAKER_PIN, HIGH);
          delay(80);
          digitalWrite(SPEAKER_PIN, LOW);
          delay(80);
          digitalWrite(SPEAKER_PIN, HIGH);
          delay(80);
          digitalWrite(SPEAKER_PIN, LOW);
          delay(80);
          digitalWrite(SPEAKER_PIN, HIGH);
        }
      }
      else if (menuTime >= 3500.0)
      {
        menuSelect = 2;
        if (menuTime == 3500.0)
        {
          setLedHex(ledHexReset[0], ledHexReset[1], ledHexReset[2], ledHexReset[3]);
          digitalWrite(SPEAKER_PIN, LOW);
          delay(80);
          digitalWrite(SPEAKER_PIN, HIGH);
          delay(80);
          digitalWrite(SPEAKER_PIN, LOW);
          delay(80);
          digitalWrite(SPEAKER_PIN, HIGH);
        }
      }
      else if (menuTime >= 1.0)
      {
        menuSelect = 1;
        if (menuTime == 1.0)
        {
          setLedHex(ledHexOffAlarm[0], ledHexOffAlarm[1], ledHexOffAlarm[2], ledHexOffAlarm[3]);
          digitalWrite(SPEAKER_PIN, LOW);
          delay(80);
          digitalWrite(SPEAKER_PIN, HIGH);
          delay(80);
        }
      }
    } while (digitalRead(BUTTON_PIN) == LOW);

    if (menuSelect == 0 || menuSelect == 1)
    {
      menuSelect = 0;
      offSpeaker = 1;
    }
    else if (menuSelect == 2)
    {
      menuSelect = 0;
      delay(200);
      ESP.reset();
    }
    else if (menuSelect == 3)
    {
      menuSelect = 0;
      eepromConfigType eepromConfigWrite;
      eepromConfigWrite.wifi.mode = 1;
      WriteEEPROMParam(eepromConfigWrite, 2);
      delay(200);
      ESP.reset();
    }

    menuTime = 0.0;
  }
}

/* Hàm đọc nhiệt độ tử cảm biến NTC */
double getTemperature(uint32_t resistance25, int beta, double resistance)
{
  double t;
  t = 1 / (((double)log((double)resistance / resistance25) / beta) + (1 / 298.15));
  t = t - 273.15;
  return t;
}

/* Hàm đọc điện trở NTC */
double getResistence(double adcValue, uint32_t adcBalanceResistor)
{
  if (adcValue == 0.0)
    return 0;
  else
    return ((double)(1023.0 * adcBalanceResistor) / adcValue - adcBalanceResistor);
}

/* Hàm in thứ ngày tháng báo thức */
void printAlarmConfig(uint8_t alarmConfig)
{
  Serial.print((((alarmConfig >> 0) & 0x01) == BIT_SET) ? "CN " : "");
  Serial.print((((alarmConfig >> 1) & 0x01) == BIT_SET) ? "T2 " : "");
  Serial.print((((alarmConfig >> 2) & 0x01) == BIT_SET) ? "T3 " : "");
  Serial.print((((alarmConfig >> 3) & 0x01) == BIT_SET) ? "T4 " : "");
  Serial.print((((alarmConfig >> 4) & 0x01) == BIT_SET) ? "T5 " : "");
  Serial.print((((alarmConfig >> 5) & 0x01) == BIT_SET) ? "T6 " : "");
  Serial.print((((alarmConfig >> 6) & 0x01) == BIT_SET) ? "T7 " : "");
}
