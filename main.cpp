#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include "Audio.h"
#include "SD.h"
#include "FS.h"
#include "driver/i2s.h"

//Peripheral pins
#define OnboardLED 5
#define VibraMotor 4
#define UserSwitch 22

// Audio pins
#define I2S_DOUT 26
#define I2S_BCLK 27
#define I2S_LRC 14

// SD CARD SPI pins
#define SD_CS 21
#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23

// EC200 PINS
#define EC_PWR 33
#define EC_RST 32
#define EC_RX 16
#define EC_TX 17

//private function declare
void receiveATCommand();
void playFile(const char *filename, unsigned long duration);
void sendATCommand(String command);

HardwareSerial LTE_Serial(2);
Audio audio;
unsigned long audioStartTime = 0;
unsigned long audioDuration = 0;
unsigned int mainFlag = 0;
char findJson[400];

void playFile(const char *filename, unsigned long duration)
{
  audio.connecttoFS(SD, filename);
  audioStartTime = millis();
  audioDuration = duration;
  Serial.print("Trying to play song");
}

void sendATCommand(String command)
{
  Serial.print("Query:    ");
  Serial.println(command);
  LTE_Serial.println(command); // Send AT command
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

void receiveATCommand(int flag)
{
  //flag=0 means Net configuration mode
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

  //flag=2 means AWS configuration mode
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
    }
  }

  //flag=1 means permanent receive mode
  else if (flag == 1)
  {

    if (LTE_Serial.available())
    {
      if (LTE_Serial.available())
      {
        String response2 = LTE_Serial.readString();
        Serial.print("Response: ");
        Serial.println(response2);
        Serial.println("");

        const char *jsonString = parseResponse(response2.c_str());
        String songName = processJsonMessage(jsonString);

        playFile(songName.c_str(), 60000);
      }
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
  sendATCommand("AT+QMTSUB=0,1,\"adit/DisasterAlertPrototype\",1");
  delay(5000);
  receiveATCommand(2);
  delay(500);
  Serial.println("Entering into Receive state permanantly.....");
  playFile("/Anime powerup.m4r", 5000);
}

void setup()
{
  Serial.begin(115200);
  LTE_Serial.begin(115200, SERIAL_8N1, EC_RX, EC_TX);
  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH);
  delay(1000);
  digitalWrite(33, LOW);
  delay(500);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SD.begin(SD_CS);
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(30);

  connectToNet();
  connectToAWS();
}

int toPlayOrNotPlay()
{
  if (audioStartTime != 0 && (millis() - audioStartTime) <= audioDuration)
  {
    String trash = LTE_Serial.readString();
    LTE_Serial.flush();
    return 1;
  }
  else
  {
    audioStartTime = 0;
    audioDuration = 0;
    audio.stopSong();
    return 0;
  }
}

void loop()
{

  mainFlag = toPlayOrNotPlay();
  if (mainFlag == 0)
  {
    receiveATCommand(1);
  }
  else if (mainFlag == 1)
  {
    audio.loop();
  }
}
