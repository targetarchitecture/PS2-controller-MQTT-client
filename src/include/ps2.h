#include <PS2X_lib.h> //for v1.6

/******************************************************************
 * set pins connected to PS2 controller:
 *   - 1e column: original
 *   - 2e colmun: Stef?
 * replace pin numbers by the ones you use
 ******************************************************************/
#define PS2_CLK D5 // SCK (YELLOW)
#define PS2_CMD D7 // MOSI (BLUE)
#define PS2_DAT D6 // MISO (GREEN)
#define PS2_SEL D8 // SS (BROWN)

/******************************************************************
 * select modes of PS2 controller:
 *   - pressures = analog reading of push-butttons
 *   - rumble    = motor rumbling
 * uncomment 1 of the lines for each mode selection
 ******************************************************************/
#define pressures false
#define rumble true

PS2X ps2x; // create PS2 Controller Class

extern unsigned int dial;

unsigned long lastCommandSentMillis = 0;

// void dealWithButton(uint16_t button, std::string topic);
void printToSerial(std::stringstream msg);
void buttonsMinimised();

void setUpPS2()
{
    // added delay to give wireless ps2 module some time to startup, before configuring it
    delay(300);
    int maxAttempts = 60;

    for (int i = 0; i <= maxAttempts; i++) // looping
    {
        std::stringstream ps2xErrorMsg;
        int ps2xError = -1;

        // setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
        ps2xError = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);

        if (ps2xError == 0)
        {
            ps2xErrorMsg << "Found Controller, configured successful (loop " << i << ")";
        }
        else if (ps2xError == 1)
        {
            ps2xErrorMsg << "No controller found  (loop " << i << ")";
        }
        else if (ps2xError == 2)
        {
            ps2xErrorMsg << "Controller found but not accepting commands  (loop " << i << ")";
        }
        else if (ps2xError == 3)
        {
            ps2xErrorMsg << "Controller refusing to enter Pressures mode, may not support it  (loop " << i << ")";
        }

#ifdef PRINT_TO_SERIAL
        Serial.println(ps2xErrorMsg.str().c_str());
#endif

        MQTTClient.publish(MQTT_INFO_TOPIC.c_str(), ps2xErrorMsg.str().c_str());
        MQTTClient.publish(MQTT_ERROR_TOPIC.c_str(), ps2xErrorMsg.str().c_str());

        if (ps2xError == 0)
        {
            // MQTTClient.publish(MQTT_CONNECTED_TOPIC, "1");

            return; // leave the setup function
        }
        else
        {
            // MQTTClient.publish(MQTT_CONNECTED_TOPIC, "0");
        }

        delay(1000);
    }

    std::stringstream ps2xMsg;
    ps2xMsg << "Time to reboot after " << maxAttempts << " attempts to connect controller";

    MQTTClient.publish(MQTT_INFO_TOPIC.c_str(), ps2xMsg.str().c_str());

    delay(500);

    // well it's probably time for a reboot
    ESP.restart();
}

void loopPS2(byte vibrate)
{
    /* You must Read Gamepad to get new values and set vibration values
     ps2x.read_gamepad(small motor on/off, larger motor strength from 0-255)
     if you don't enable the rumble, use ps2x.read_gamepad(); with no values
     You should call this at least once a second
   */

    // read controller and set large motor to spin at 'vibrate' speed
    ps2x.read_gamepad(false, vibrate);

    long left_x_mapped = map(ps2x.Analog(PSS_LX), 0, 255, -100, 100);
    long left_y_mapped = map(ps2x.Analog(PSS_LY), 0, 255, -100, 100);

    long right_x_mapped = map(ps2x.Analog(PSS_RX), 0, 255, -100, 100);
    long right_y_mapped = map(ps2x.Analog(PSS_RY), 0, 255, -100, 100);

    // Serial.print("PSS_LX:" + String(left_x_mapped) + " PSS_LY:" + String(left_y_mapped));
    // Serial.println("PSS_RX:" + String(right_x_mapped) + " PSS_RY:" + String(right_y_mapped));

    // check for centered zone and ignore
    if (abs(left_x_mapped) > 5 ||
        abs(left_y_mapped) > 5)
    {
        MQTTClient.publish(MQTT_LEFT_X_TOPIC.c_str(), String(left_x_mapped).c_str());
        MQTTClient.publish(MQTT_LEFT_X_RAW_TOPIC.c_str(), String(ps2x.Analog(PSS_LX)).c_str());
        MQTTClient.publish(MQTT_LEFT_Y_TOPIC.c_str(), String(left_y_mapped).c_str());
        MQTTClient.publish(MQTT_LEFT_Y_RAW_TOPIC.c_str(), String(ps2x.Analog(PSS_LY)).c_str());

        digitalWrite(LED_BUILTIN, LOW); // set LED to flash on
    }

    // check for centered zone and ignore
    if (abs(right_x_mapped) > 5 ||
        abs(right_y_mapped) > 5)
    {
        MQTTClient.publish(MQTT_RIGHT_X_TOPIC.c_str(), String(right_x_mapped).c_str());
        MQTTClient.publish(MQTT_RIGHT_X_RAW_TOPIC.c_str(), String(ps2x.Analog(PSS_RX)).c_str());
        MQTTClient.publish(MQTT_RIGHT_Y_TOPIC.c_str(), String(right_y_mapped).c_str());
        MQTTClient.publish(MQTT_RIGHT_Y_RAW_TOPIC.c_str(), String(ps2x.Analog(PSS_RY)).c_str());

        digitalWrite(LED_BUILTIN, LOW); // set LED to flash on
    }

    // publish buttons minimised
    buttonsMinimised();
}

void buttonsMinimised()
{
    bool btnPressed = false;

    uint16_t buttonIDs[] = {PSB_START, PSB_SELECT, PSB_PAD_UP, PSB_PAD_DOWN, PSB_PAD_LEFT, PSB_PAD_RIGHT, PSB_TRIANGLE, PSB_CROSS, PSB_SQUARE, PSB_CIRCLE, PSB_L1, PSB_R1, PSB_L2, PSB_R2, PSB_L3, PSB_R3};
    String buttonTxt[] = {"START", "SELECT", "PAD_UP", "PAD_DOWN", "PAD_LEFT", "PAD_RIGHT", "TRIANGLE", "CROSS", "SQUARE", "CIRCLE", "BUMPER_LEFT", "BUMPER_RIGHT", "TRIGGER_LEFT", "TRIGGER_RIGHT", "STICK_LEFT", "STICK_RIGHT"};

    for (size_t i = 0; i < 16; i++)
    {
        if (ps2x.Button(buttonIDs[i]) == true)
        {
            String topic = MQTT_BUTTON_TOPIC + String("/") + buttonTxt[i];
            topic.toLowerCase();

            MQTTClient.publish(topic.c_str(), "press");

            btnPressed = true;
        }
    }

    if (btnPressed == true)
    {
        digitalWrite(LED_BUILTIN, LOW); // set LED to flash on

        lastCommandSentMillis = millis(); // reset timer
    }
}
