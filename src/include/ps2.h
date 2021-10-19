#include <PS2X_lib.h> //for v1.6

#define ARDUINOJSON_ENABLE_STD_STRING 1
#include <ArduinoJSON.h>

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

//create the joystick object once
const size_t left_stick_capacity = JSON_OBJECT_SIZE(4);
DynamicJsonDocument left_joystick(left_stick_capacity);

const size_t right_stick_capacity = JSON_OBJECT_SIZE(4);
DynamicJsonDocument right_joystick(right_stick_capacity);

const size_t button_capacity = JSON_OBJECT_SIZE(21);
DynamicJsonDocument buttons(button_capacity);

void dealWithButton(uint16_t button, std::string topic);
void printToSerial(std::stringstream msg);

void setUpPS2()
{
    //added delay to give wireless ps2 module some time to startup, before configuring it
    delay(300);
    int maxAttempts = 60;

    for (int i = 0; i <= maxAttempts; i++) //looping
    {
        std::stringstream ps2xErrorMsg;
        int ps2xError = -1;

        //setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
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

        MQTTClient.publish(MQTT_INFO_TOPIC, ps2xErrorMsg.str().c_str());
        MQTTClient.publish(MQTT_ERROR_TOPIC, ps2xErrorMsg.str().c_str());

        if (ps2xError == 0)
        {
            //MQTTClient.publish(MQTT_CONNECTED_TOPIC, "1");

            return; //leave the setup function
        }
        else
        {
            //MQTTClient.publish(MQTT_CONNECTED_TOPIC, "0");
        }

        delay(1000);
    }

    std::stringstream ps2xMsg;
    ps2xMsg << "Time to reboot after " << maxAttempts << " attempts to connect controller";

    MQTTClient.publish(MQTT_INFO_TOPIC, ps2xMsg.str().c_str());

    delay(500);

    //well it's probably time for a reboot
    ESP.restart();
}



void loopPS2(byte vibrate)
{
    /* You must Read Gamepad to get new values and set vibration values
     ps2x.read_gamepad(small motor on/off, larger motor strength from 0-255)
     if you don't enable the rumble, use ps2x.read_gamepad(); with no values
     You should call this at least once a second
   */

    // bool turnOnLEDtoShowMQTTMessage = false;

    //read controller and set large motor to spin at 'vibrate' speed
    ps2x.read_gamepad(false, vibrate);

    long left_x_mapped = map(ps2x.Analog(PSS_LX), 0, 255, -100, 100);
    long left_y_mapped = map(ps2x.Analog(PSS_LY), 0, 255, -100, 100);

    long right_x_mapped = map(ps2x.Analog(PSS_RX), 0, 255, -100, 100);
    long right_y_mapped = map(ps2x.Analog(PSS_RY), 0, 255, -100, 100);

    //Serial.print("PSS_LX:" + String(left_x_mapped) + " PSS_LY:" + String(left_y_mapped));
    //Serial.println("PSS_RX:" + String(right_x_mapped) + " PSS_RY:" + String(right_y_mapped));

    //check for centered zone and ignore
    if (abs(left_x_mapped) > 5 ||
        abs(left_y_mapped) > 5)
    {
        left_joystick["x_mapped"] = left_x_mapped;
        left_joystick["y_mapped"] = left_y_mapped;

        left_joystick["x"] = ps2x.Analog(PSS_LX);
        left_joystick["y"] = ps2x.Analog(PSS_LY);

        std::string json;

        serializeJson(left_joystick, json);

        MQTTClient.publish(MQTT_LEFT_TOPIC, json.c_str());

        digitalWrite(LED_BUILTIN, LOW); //set LED to flash on

        left_joystick.clear();
    }

    //check for centered zone and ignore
    if (abs(right_x_mapped) > 5 ||
        abs(right_y_mapped) > 5)
    {
        right_joystick["x_mapped"] = right_x_mapped;
        right_joystick["y_mapped"] = right_y_mapped;

        right_joystick["x"] = ps2x.Analog(PSS_RX);
        right_joystick["y"] = ps2x.Analog(PSS_RY);

        std::string json;

        serializeJson(right_joystick, json);

        MQTTClient.publish(MQTT_RIGHT_TOPIC, json.c_str());

        digitalWrite(LED_BUILTIN, LOW); //set LED to flash on

        right_joystick.clear();
    }

    //clear down button array
    buttons.clear();

    dealWithButton(PSB_START, "start");
    dealWithButton(PSB_SELECT, "ps2_select");
    dealWithButton(PSB_PAD_UP, "pad_up");
    dealWithButton(PSB_PAD_DOWN, "pad_down");
    dealWithButton(PSB_PAD_LEFT, "pad_left");
    dealWithButton(PSB_PAD_RIGHT, "pad_right");
    dealWithButton(PSB_TRIANGLE, "triangle");
    dealWithButton(PSB_CROSS, "cross");
    dealWithButton(PSB_SQUARE, "square");
    dealWithButton(PSB_START, "start");
    dealWithButton(PSB_CIRCLE, "circle");
    dealWithButton(PSB_L1, "shoulder_left");
    dealWithButton(PSB_R1, "shoulder_right");
    dealWithButton(PSB_L2, "trigger_left");
    dealWithButton(PSB_R2, "trigger_right");
    dealWithButton(PSB_CROSS, "cross");
    dealWithButton(PSB_SQUARE, "square");
    dealWithButton(PSB_START, "start");
    dealWithButton(PSB_L3, "stick_left");
    dealWithButton(PSB_R3, "stick_right");

    JsonObject buttonObj = buttons.as<JsonObject>();

    // size_t joystickObjSize = joystickObj.size();
    // Serial.println(joystickObjSize);

    if (buttonObj.size() > 0)
    {
        std::string json;

        serializeJson(buttonObj, json);

        MQTTClient.publish(MQTT_BUTTON_TOPIC, json.c_str());

        lastCommandSentMillis = millis(); //reset timer
    }
}

void dealWithButton(uint16_t button, std::string topic)
{
    if (ps2x.Button(button) == true)
    {
        buttons[topic] = true;

        digitalWrite(LED_BUILTIN, LOW); //set LED to flash on
    }
    else
    {
        if (buttons.containsKey(topic) == true)
        {
            buttons.remove(topic);
        }
    }
}