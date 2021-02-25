#include <Arduino.h>
#include <string>
#include <vars.h>
#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h> // Include the mDNS library
//#include <DNSServer.h>   //Local DNS Server used for redirecting all requests to the configuration portal
#include "credentials.h"
#include <mqttClient.h>
#include <ps2.h>
//#include <ArduinoOTA.h>

// declare objects & variables
void setupWifi();
void setOTA();
void getSwitchValue();

unsigned int dial = -1;

unsigned long interval = 1000 * 60 * 10; //10 minutes for a reboot if no signal recieved

void setup()
{
  pinMode(A0, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);

  randomSeed(micros());

  setupWifi();

  setupMQTTClient();

  //String SSID = WIFI_SSID;
  // publishMQTTmessage("Connected to SSID: " + SSID);
  // publishMQTTmessage("IP address: " + WiFi.localIP().toString());

  MQTTClient.publish(MQTT_IP_TOPIC, WIFI_SSID);
  MQTTClient.publish(MQTT_IP_TOPIC, WiFi.localIP().toString().c_str());

  getSwitchValue(); //get switch value once

  //setOTA();

  setUpPS2(); //connect controller

  MQTT_RUMBLE = 0;
}

void loop()
{
  //ArduinoOTA.handle();

  //MDNS.update();

  loopPS2(MQTT_RUMBLE);

  unsigned long currentMillis = millis();

  //stop rumble after 500ms if no MQTT signal recieved
  if (currentMillis - lastRumbleCommandRecievedMillis > 500)
  {
    lastRumbleCommandRecievedMillis = currentMillis;
    MQTT_RUMBLE = 0;
  }

  //delay(50); //produces 17-19 messages per second
  delay(100); //produces 9-10 messages per second

  //MQTT section
  if (!MQTTClient.connected())
  {
    reconnect();
  }

  MQTTClient.loop();

  //check for in-activity
  if (currentMillis - lastCommandSentMillis > interval)
  {
    publishMQTTmessage("Time to reboot due to inactivity");
    delay(500);

    //well it's probably time for a reboot
    ESP.restart();
  }

  //set LED off
  digitalWrite(LED_BUILTIN, HIGH);
}

void getSwitchValue()
{
  const int numReadings = 10;

  int total = 0; // the running total

  for (int thisReading = 0; thisReading < numReadings; thisReading++)
  {
    int sensorValue = analogRead(A0);

    total = total + sensorValue;

    delay(100);
  }

  // calculate the average:
  int average = total / numReadings;

  Serial.print("Raw dial value:");
  Serial.println(average);

  if (average < 200)
  {
    dial = 1;
  }
  else if ((200 < average) && (average < 350))
  {
    dial = 2;
  }
  else if ((350 < average) && (average < 800))
  {
    dial = 3;
  }
  else
  {
    dial = 4;
  }

  Serial.print("Dial position:");
  Serial.println(dial);
}

void setOTA()
{
  // ArduinoOTA.setHostname(MDNS_HOSTNAME);
  // ArduinoOTA.setPassword(OTA_PASSWORD);

  // ArduinoOTA.onStart([]() {
  //   Serial.println("Start");
  // });

  // ArduinoOTA.onEnd([]() {
  //   Serial.println("\nEnd");
  // });

  // ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  //   Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  // });

  // ArduinoOTA.onError([](ota_error_t error) {
  //   Serial.printf("Error[%u]: ", error);
  //   if (error == OTA_AUTH_ERROR)
  //     Serial.println("Auth Failed");
  //   else if (error == OTA_BEGIN_ERROR)
  //     Serial.println("Begin Failed");
  //   else if (error == OTA_CONNECT_ERROR)
  //     Serial.println("Connect Failed");
  //   else if (error == OTA_RECEIVE_ERROR)
  //     Serial.println("Receive Failed");
  //   else if (error == OTA_END_ERROR)
  //     Serial.println("End Failed");
  // });
  // ArduinoOTA.begin();
}

void setupWifi()
{
  //sort out WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Connect to the network

  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    ESP.restart();
  }

  Serial.println("Ready on the local network");
  Serial.println("IP address: " + WiFi.localIP().toString());

  // if (!MDNS.begin(MDNS_HOSTNAME))
  // { // Start the mDNS responder for esp8266.local
  //   Serial.println("Error setting up MDNS responder!");
  // }
  //Serial.println("mDNS responder started");
}
