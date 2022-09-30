#include <Arduino.h>
#include <sstream>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <mqttClient.h>
#include <ps2.h>
#include "credentials.h"
#include "topics.h"
#include "vars.h"
#include <Preferences.h>

#define PRINT_TO_SERIAL 1

// declare objects & variables
void setupWifi();
void getSwitchValue();
void onWifiConnect(const WiFiEventStationModeGotIP &event);
void onWifiDisconnect(const WiFiEventStationModeDisconnected &event);

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

Preferences preferences;

unsigned long loopDelay = 100;

unsigned int dial = -1;

unsigned long interval = 1000 * 60 * 10; // 10 minutes for a reboot if no signal recieved

bool sendNetworkDetails = true;

unsigned long lastDelayLoopMillis = 0;

void setup()
{
  pinMode(A0, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

#ifdef PRINT_TO_SERIAL
  Serial.begin(115200);
#endif

  // get loop delay
  preferences.begin("ps2", false);
  loopDelay = preferences.getULong("loopDelay", loopDelay);

  randomSeed(micros());

  setupWifi();

  setupMQTTClient();

  getSwitchValue(); // get switch value once

  setUpPS2(); // connect controller

  MQTT_RUMBLE = 0;
}

void loop()
{
  // delay(50);  //produces 17-19 messages per second
  // delay(100); //produces 9-10 messages per second
  // delay(200); //produces ~5 messages per second
  delay(loopDelay); // now variable

  loopPS2(MQTT_RUMBLE);

  // stop rumble after 500ms if no MQTT signal recieved
  unsigned long currentMillis = millis();

  if (currentMillis - lastRumbleCommandRecievedMillis > 500)
  {
    lastRumbleCommandRecievedMillis = currentMillis;
    MQTT_RUMBLE = 0;
  }

  if (WiFi.isConnected() == false)
  {
    // just do nothing and wait for the reconnection event to happen
    delay(1000);
  }
  else
  {
    // MQTT section
    if (MQTTClient.connected() == false)
    {
      MQTTConnect();
    }
    else
    {
      if (sendNetworkDetails == true)
      {
        // MQTTClient.publish(MQTT_IP_TOPIC, WIFI_SSID);
        MQTTClient.publish(MQTT_IP_TOPIC.c_str(), WiFi.localIP().toString().c_str());

        sendNetworkDetails = false;
      }

      // send loop delay
      if (currentMillis - lastDelayLoopMillis > 5000)
      {
        lastDelayLoopMillis = currentMillis;
        MQTTClient.publish(MQTT_LOOP_DELAY_TOPIC.c_str(), String(loopDelay).c_str());
      }
    }

    MQTTClient.loop();
  }

  // check for in-activity
  if (currentMillis - lastCommandSentMillis > interval)
  {
    MQTTClient.publish(MQTT_INFO_TOPIC.c_str(), "Time to reboot due to inactivity");

    delay(500);

    // well it's probably time for a reboot
    ESP.restart();
  }

  // set LED off
  digitalWrite(LED_BUILTIN, HIGH);
}

void getSwitchValue()
{
  const int numReadings = 100;

  int total = 0; // the running total

  for (int thisReading = 0; thisReading < numReadings; thisReading++)
  {
    int sensorValue = analogRead(A0);

    total = total + sensorValue;

    delay(10);
  }

  // calculate the average:
  int average = total / numReadings;

#ifdef PRINT_TO_SERIAL
  Serial.print("Raw dial value:");
  Serial.println(average);
#endif

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

#ifdef PRINT_TO_SERIAL
  Serial.print("Dial position:");
  Serial.println(dial);
#endif

  std::stringstream msg;
  msg << dial;

  MQTTClient.publish(MQTT_DIAL_TOPIC.c_str(), msg.str().c_str());
}

void setupWifi()
{
  // Register event handlers
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  // sort out WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Connect to the network
}

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
  sendNetworkDetails = true;

#ifdef PRINT_TO_SERIAL
  Serial.println("Connected to Wi-Fi sucessfully.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
#ifdef PRINT_TO_SERIAL
  Serial.println("Disconnected from Wi-Fi, trying to connect...");
#endif
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Reconnect to the network
}