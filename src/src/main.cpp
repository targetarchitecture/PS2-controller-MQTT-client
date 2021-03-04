#include <Arduino.h>
#include <sstream>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <mqttClient.h>
#include <ps2.h>
#include "credentials.h"
#include "topics.h"
#include "vars.h"

// declare objects & variables
void setupWifi();
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

  MQTTClient.publish(MQTT_IP_TOPIC, WIFI_SSID);
  MQTTClient.publish(MQTT_IP_TOPIC, WiFi.localIP().toString().c_str());

  getSwitchValue(); //get switch value once

  setUpPS2(); //connect controller

  MQTT_RUMBLE = 0;
}

void loop()
{
  loopPS2(MQTT_RUMBLE);

  //stop rumble after 500ms if no MQTT signal recieved
  unsigned long currentMillis = millis();

  if (currentMillis - lastRumbleCommandRecievedMillis > 500)
  {
    lastRumbleCommandRecievedMillis = currentMillis;
    MQTT_RUMBLE = 0;
  }

  //delay(50); //produces 17-19 messages per second
  //delay(100); //produces 9-10 messages per second
  delay(200); //produces ~5 messages per second

  if (WiFi.isConnected() == false)
  {
    setupWifi();
  }

  //MQTT section
  if (!MQTTClient.connected())
  {
    reconnect();
  }

  MQTTClient.loop();

  //check for in-activity
  if (currentMillis - lastCommandSentMillis > interval)
  {
    MQTTClient.publish(MQTT_INFO_TOPIC, "Time to reboot due to inactivity");

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

  std::stringstream msg;
  msg << dial;

  MQTTClient.publish(MQTT_DIAL_TOPIC, msg.str().c_str());
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
}
