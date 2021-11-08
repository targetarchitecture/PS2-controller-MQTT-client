#include <PubSubClient.h>
#include "topics.h"
#include "vars.h"
#include "credentials.h"
#include <sstream>

byte MQTT_RUMBLE = 0;
unsigned long lastRumbleCommandRecievedMillis = 0;
unsigned long connectionTiming = 0;
void callback(char *topic, byte *payload, unsigned int length);

void setupMQTTClient()
{
#ifdef PRINT_TO_SERIAL
  Serial.println("Connecting to MQTT server");
#endif

  //set this to be a large enough value to allow a large MQTT message
  MQTTClient.setBufferSize(5000);

  MQTTClient.setServer(MQTT_SERVER, 1883);

  // setup callbacks
  MQTTClient.setCallback(callback);
}

void MQTTConnect()
{
    yield();

#ifdef PRINT_TO_SERIAL
    Serial.print("Attempting MQTT connection...");
#endif

    connectionTiming = millis();

    // Attempt to connect
    if (MQTTClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_KEY))
    {
#ifdef PRINT_TO_SERIAL
      Serial.println("MQTT connected");
      Serial.print("connected in ");
      Serial.print(millis() - connectionTiming);
      Serial.println("ms");
#endif

      // Once connected, publish an announcement...
      MQTTClient.publish(MQTT_INFO_TOPIC, "Connected");

// ... and resubscribe
#ifdef PRINT_TO_SERIAL
      Serial.println("resubscribing");
#endif

      if (MQTTClient.subscribe(MQTT_RUMBLE_TOPIC) == true)
      {
#ifdef PRINT_TO_SERIAL
        Serial.println("subscribed");
#endif
      }
      else
      {
#ifdef PRINT_TO_SERIAL
        Serial.println("NOT subscribed");
#endif
      }
    }
    else
    {
#ifdef PRINT_TO_SERIAL
      Serial.print("failed, rc=");
      Serial.print(MQTTClient.state());
      Serial.println(" try again in 5 seconds");
#endif
    }
  
}

void callback(char *topic, byte *payload, unsigned int length)
{
#ifdef PRINT_TO_SERIAL
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
#endif

  std::string message = "";

  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

#ifdef PRINT_TO_SERIAL
  Serial.println(message.c_str());
#endif

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
