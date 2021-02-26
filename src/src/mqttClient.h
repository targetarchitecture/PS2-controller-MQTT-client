#include <PubSubClient.h>
#include "topics.h"
#include "vars.h"
#include "credentials.h"

byte MQTT_RUMBLE = 0;
unsigned long lastRumbleCommandRecievedMillis = 0;
void callback(char *topic, byte *payload, unsigned int length);

void setupMQTTClient()
{
  Serial.println("Connecting to MQTT server");

  //set this to be a large enough value to allow a large MQTT message
  MQTTClient.setBufferSize(5000);

  MQTTClient.setServer(MQTT_SERVER, 1883);

  // setup callbacks
  MQTTClient.setCallback(callback);

  Serial.println("connect mqtt...");

  if (MQTTClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_KEY))
  {
    Serial.println("MQTT Connected");
    MQTTClient.publish(MQTT_INFO_TOPIC, "Connected");

    Serial.println("subscribing");

    if (MQTTClient.subscribe(MQTT_RUMBLE_TOPIC) == true)
    {
      Serial.println("subscribed");
    }
    else
    {
      Serial.println("NOT subscribed");
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!MQTTClient.connected())
  {
    yield();

    Serial.print("Attempting MQTT connection...");

    // Attempt to connect
    if (MQTTClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_KEY))
    {
      Serial.println("MQTT connected");
      // Once connected, publish an announcement...
      MQTTClient.publish(MQTT_INFO_TOPIC, "Reconnected");

      // ... and resubscribe
      Serial.println("resubscribing");

      if (MQTTClient.subscribe(MQTT_RUMBLE_TOPIC) == true)
      {
        Serial.println("subscribed");
      }
      else
      {
        Serial.println("NOT subscribed");
      }
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(MQTTClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  std::string message = "";

  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  Serial.println(message.c_str());

  if (std::string(topic) == std::string(MQTT_RUMBLE_TOPIC))
  {
    int RumbleStrength = atoi(message.c_str());

    MQTT_RUMBLE = map(RumbleStrength, 0, 100, 0, 255); //translate 0-100% to 0-255 bytes
    lastRumbleCommandRecievedMillis = millis();

    std::stringstream ps2xMsg;
    ps2xMsg << "Vibrate set to " << (int)MQTT_RUMBLE;

    MQTTClient.publish(MQTT_INFO_TOPIC, ps2xMsg.str().c_str());
  }
}
