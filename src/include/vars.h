#pragma once

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

WiFiClient wifiConnection;
PubSubClient MQTTClient(wifiConnection);