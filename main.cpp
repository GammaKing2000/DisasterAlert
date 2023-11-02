#include "Arduino.h"
#include <ArduinoJson.h>
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "HardwareSerial.h"

// microSD Card Reader connections
#define I2S_DOUT 26
#define I2S_BCLK 27
#define I2S_LRC 14

// SD CARD SPI pins
#define SD_CS 21
#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23

#define EC_RX 16
#define EC_TX 17

//peripheral devices
#define UserSwitch 22
#define VibraMotor 4
#define OnboardLED 5

HardwareSerial LTE_Serial(2);
Audio audio;

const String DEVICE_ID = "1";

unsigned long startTime = 0;
unsigned int duration = 0;
unsigned int mainFlag = 0;
unsigned long flagChangeTime = 0;

unsigned long previousMillis = 0;
const long interval = 1000;

int buttonState = HIGH;
int lastButtonState = HIGH;
char findJson[400];

void receiveATCommand(int flag);
void sendATCommand(String command);

void vibrate(uint8_t pin, long interval)
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    if (digitalRead(pin) == HIGH)
    {
      digitalWrite(pin, LOW);
    }
    else
    {
      digitalWrite(pin, HIGH);
    }
  }
}

void sendATCommand(String command)
{
  Serial.print("Query:    ");
  Serial.println(command);
  LTE_Serial.println(command);
}

const char *parseResponse(const char *response)
{
  const char *jsonStart = strchr(response, '{');

  if (jsonStart)
  {
    return jsonStart;
  }
  else
  {
    return nullptr;
  }
}

const char *parseLOCResponse(const char *response)
{
  const char *jsonStart = strchr(response, ',') + 1;

  if (jsonStart)
  {
    return jsonStart;
  }
  else
  {
    return nullptr;
  }
}

String processJsonMessage(const char *jsonString)
{
  if (jsonString)
  {
    StaticJsonDocument<400> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, jsonString);
    if (!error)
    {
      const char *message = jsonDoc["message"];
      String temp(message);
      if (message)
      {
        Serial.print("Received message: ");
        Serial.println(message);
      }
      else
      {
        Serial.println("No message field in the JSON.");
      }
      return temp;
    }
    else
    {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.c_str());
    }
  }
  else
  {
    Serial.println("No JSON part found in the response.");
  }
  return "";
}

void checkLOC()
{
  sendATCommand("AT+QGPS=1,30,50,1,300");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QGPSLOC=0");
  delay(2000);
  receiveATCommand(3);
  delay(500);
  sendATCommand("AT+QGPSEND");
  delay(1000);
  receiveATCommand(0);
  delay(500);
}

void Publish_Message(const char *jsonString)
{
  String output = "";
  StaticJsonDocument<96> doc;
  //          <UTC time> <ddmm.mmmm>  <ddmm,mmmm> <HDOP> <altitude>                  <date>
  //+QGPSLOC: 061951.000, 3150.7223N, 11711.9293E, 0.7,    62.2,   2,000.00,0.0,0.0, 110513, 09
  String Lat = "";
  String Long = "";

  while (*jsonString != ',')
  {
    Lat = Lat + *jsonString;
    jsonString++;
  }
  jsonString++;
  while (*jsonString != ',')
  {
    Long = Long + *jsonString;
    jsonString++;
  }
  doc["DEVICE_ID"] = DEVICE_ID;
  doc["LAT"] = Lat;
  doc["LONG"] = Long;

  serializeJson(doc, output);
  int payloadSize = output.length();
  sendATCommand("AT+QMTPUBEX=0,1,1,0,\"AWS/CIER/INFO/1\"," + String(payloadSize));
  delay(500);
  sendATCommand(output);
  delay(2000);
  receiveATCommand(0);
}

void Publish_LIVE_NOW()
{
  String output = "";
  StaticJsonDocument<96> doc;

  doc["DEVICE_ID"] = DEVICE_ID;
  doc["STATUS"] = "ACTIVE";

  serializeJson(doc, output);
  int payloadSize = output.length();

  sendATCommand("AT+QMTPUBEX=0,1,1,0,\"AWS/CIER/INFO/1\"," + String(payloadSize));
  delay(500);
  sendATCommand(output);
  delay(2000);
  receiveATCommand(0);
}

void receiveATCommand(int flag)
{
  // flag=0 means Net configuration mode
  if (flag == 0)
  {
  retry:
    if (LTE_Serial.available())
    {
      String response1 = LTE_Serial.readStringUntil('\n');
      String response2 = LTE_Serial.readString();
      Serial.print("Response: ");
      Serial.println(response2);
      Serial.println("");
      if (response2 == "NO SIM")
      {
        Serial.println("Retrying query...");
        sendATCommand("AT+CPIN?");
        delay(1000);
        goto retry;
      }
    }
  }

  // flag=1 means permanent receive mode
  else if (flag == 1)
  {
    if (LTE_Serial.available())
    {
      String response2 = LTE_Serial.readString();
      Serial.print("Response: ");
      Serial.println(response2);
      Serial.println("");

      const char *jsonString = parseResponse(response2.c_str());
      String songName = processJsonMessage(jsonString);

      if (songName == "1")
      {
        mainFlag = 1;
        audio.connecttoFS(SD, "/EARTHQUAKE.mp3");
        vibrate(VibraMotor, 2000);
      }
      if (songName == "2")
      {
        mainFlag = 1;
        audio.connecttoFS(SD, "/FLOOD.mp3");
        vibrate(VibraMotor, 2000);
      }
      if (songName == "3")
      {
        mainFlag = 1;
        audio.connecttoFS(SD, "/LANDSLIDE.mp3");
        vibrate(VibraMotor, 2000);
      }
      if (songName == "4")
      {
        mainFlag = 1;
        audio.connecttoFS(SD, "/LIGHTENINGSTRIKE.mp3");
        vibrate(VibraMotor, 2000);
      }
      if (songName == "5")
      {
        mainFlag = 1;
        audio.connecttoFS(SD, "/THUNDERSTORM.mp3");
        vibrate(VibraMotor, 2000);
      }
      if (songName == "LOC")
      {
        checkLOC();
      }
      if (songName == "STATUS")
      {
        Publish_LIVE_NOW();
      }
    }
  }

  // flag=2 means AWS configuration mode
  else if (flag == 2)
  {
    if (LTE_Serial.available())
    {
      String response1 = LTE_Serial.readStringUntil('\n');
      String response2 = LTE_Serial.readString();
      Serial.print("Response: ");
      Serial.println(response2);
      Serial.println("");
      if (response2 == "ERROR")
      {
        ESP.restart();
      }
      if (response2.indexOf("+QMTSUB: 0,1,0,1") != -1)
      {
        Serial.println("Received the desired message!");
      }
      else
      {
        Serial.println("retry");
        delay(1000); // Adjust delay as needed
      }
    }
  }

  //publishing GPS location
  else if (flag == 3)
  {
    if (LTE_Serial.available())
    {
      String response2 = LTE_Serial.readString();
      Serial.print("Response: ");
      Serial.println(response2);
      Serial.println("");
      const char *jsonString = parseLOCResponse(response2.c_str());
      Publish_Message(jsonString);
    }
  }
}

void connectToNet()
{
  sendATCommand("AT");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+CPIN?");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+CREG?");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+CGREG?");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+CSQ");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+COPS?");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+CGREG?");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",0");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QIACT=1");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QIACT?");
  delay(1000);
  receiveATCommand(0);
  delay(500);
}

void connectToGPS()
{
  sendATCommand("AT+QGPSCFG=\"nmeasrc\",1");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QGPSCFG=\"gpsnmeatype\"=2");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QGPSCFG=\"glonassnmeatype\",0");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QGPSCFG=\"galileonmeatype\",0");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QGPSCFG=\"beidounmeatype\",0");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QGPSCFG=\"gnssnmeatype\",2");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QGPSCFG=\"gnssconfig\",0");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QGPSCFG=\"autogps\",0");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QGPSCFG=\"gnssconfig\",0");
  delay(1000);
  receiveATCommand(0);
  delay(500);
}

void connectToAWS()
{
  sendATCommand("AT+QMTCFG=\"recv/mode\",0,0,1");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QMTCFG=\"SSL\",0,1,2");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QSSLCFG=\"cacert\",2,\"UFS:cacert.pem\"");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QSSLCFG=\"clientcert\",2,\"UFS:client.pem\"");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QSSLCFG=\"clientkey\",2,\"UFS:user_key.pem\"");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QSSLCFG=\"seclevel\",2,2");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QSSLCFG=\"sslversion\",2,4");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QSSLCFG=\"ciphersuite\",2,0xFFFF");
  delay(1000);
  receiveATCommand(0);
  delay(500);
  sendATCommand("AT+QSSLCFG=\"ignorelocaltime\",2,1");
  delay(1000);
  receiveATCommand(2);
  delay(500);
  sendATCommand("AT+QMTOPEN=0,\"a3egi4f3zufw8w-ats.iot.us-east-1.amazonaws.com\",8883");
  delay(1000);
  receiveATCommand(2);
  delay(500);
  sendATCommand("AT+QMTCONN=0,\"M26_0206\"");
  delay(5000);
  receiveATCommand(2);
  delay(500);
  sendATCommand("AT+QMTSUB=0,1,\"AWS/CIER/SUB/1\",1");
  delay(5000);
  receiveATCommand(2);
  delay(500);
  Serial.println("Entering into Receive state permanantly.....");

  Publish_LIVE_NOW();
  vibrate(OnboardLED, 2000);
}

void setup()
{
  Serial.begin(9600);
  LTE_Serial.begin(115200, SERIAL_8N1, EC_RX, EC_TX);
  pinMode(33, OUTPUT);
  pinMode(VibraMotor, OUTPUT);
  pinMode(UserSwitch, INPUT);
  pinMode(OnboardLED, OUTPUT);
  digitalWrite(33, HIGH);
  delay(1000);
  digitalWrite(33, LOW);
  delay(500);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  if (!SD.begin(SD_CS))
  {
    Serial.println("Error accessing microSD card!");
    while (true)
      ;
  }

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

  audio.setVolume(21);
  
  connectToNet();
  connectToGPS();
  connectToAWS();
}

void loop()
{
  if (mainFlag == 0)
  {
    receiveATCommand(1);
  }
  else if (mainFlag == 1)
  {
    String trash = LTE_Serial.readString();
    LTE_Serial.flush();
    audio.loop();
  }

  buttonState = digitalRead(UserSwitch);

  if (mainFlag == 1 && buttonState == LOW && lastButtonState == HIGH)
  {
    Serial.println("BUTTON STOP");
    audio.stopSong();
    digitalWrite(VibraMotor, LOW);
    mainFlag = 0;
    checkLOC();
  }
  lastButtonState = buttonState;
}
