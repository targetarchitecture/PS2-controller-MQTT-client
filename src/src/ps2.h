#include <PS2X_lib.h> //for v1.6
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
const size_t capacity = JSON_OBJECT_SIZE(30);
DynamicJsonDocument joystick(capacity);

void setUpPS2()
{
    //added delay to give wireless ps2 module some time to startup, before configuring it
    delay(300);
    int maxAttempts = 60;

    for (int i = 0; i <= maxAttempts; i++) //looping
    {
        String ps2xErrorMsg = "";
        int ps2xError = 0;

        //setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
        ps2xError = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);

        if (ps2xError == 0)
        {
            ps2xErrorMsg = "Found Controller, configured successful (loop " + String(i) + ")";

            Serial.println(ps2xErrorMsg);

            publishMQTTmessage(ps2xErrorMsg);

            delay(1000);

            return; //leave the setup function
        }
        else if (ps2xError == 1)
        {
            ps2xErrorMsg = "No controller found (loop " + String(i) + ")";
        }
        else if (ps2xError == 2)
        {
            ps2xErrorMsg = "Controller found but not accepting commands (loop " + String(i) + ")";
        }
        else if (ps2xError == 3)
        {
            ps2xErrorMsg = "Controller refusing to enter Pressures mode, may not support it (loop " + String(i) + ")";
        }

        Serial.println(ps2xErrorMsg);

        publishMQTTmessage(ps2xErrorMsg);

        delay(1000);
    }

    publishMQTTmessage("Time to reboot after " + String(maxAttempts) + " attempts to connect controller");
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

    if (vibrate > 0)
    {
        publishMQTTmessage("Vibrate set to " + String(vibrate));
    }

    bool turnOnLEDtoShowMQTTMessage = false;

    //DualShock Controller
    ps2x.read_gamepad(false, vibrate); //read controller and set large motor to spin at 'vibrate' speed

    long left_x_mapped = map(ps2x.Analog(PSS_LX), 0, 255, -100, 100);
    long left_y_mapped = map(ps2x.Analog(PSS_LY), 0, 255, -100, 100);
    long right_x_mapped = map(ps2x.Analog(PSS_RX), 0, 255, -100, 100);
    long right_y_mapped = map(ps2x.Analog(PSS_RY), 0, 255, -100, 100);

    //Serial.print("PSS_LX:" + String(left_x_mapped) + " PSS_LY:" + String(left_y_mapped));
    //Serial.println("PSS_RX:" + String(right_x_mapped) + " PSS_RY:" + String(right_y_mapped));

    // lastCommandSentMillis

    //bool sendMessage = false;

    //check for centered zone and ignore
    if (abs(left_x_mapped) > 5 ||
        abs(left_y_mapped) > 5)
    {
        joystick["left_x_mapped"] = left_x_mapped;
        joystick["left_y_mapped"] = left_y_mapped;

        joystick["left_x"] = ps2x.Analog(PSS_LX);
        joystick["left_y"] = ps2x.Analog(PSS_LY);

        turnOnLEDtoShowMQTTMessage = true;
    }

    //check for centered zone and ignore
    if (abs(right_x_mapped) > 5 ||
        abs(right_y_mapped) > 5)
    {
        joystick["right_x_mapped"] = right_x_mapped;
        joystick["right_y_mapped"] = right_y_mapped;

        joystick["right_x"] = ps2x.Analog(PSS_RX);
        joystick["right_y"] = ps2x.Analog(PSS_RY);

        turnOnLEDtoShowMQTTMessage = true;
    }

    //only set if changed - so store the STATE !

    if (ps2x.Button(PSB_START) != joystick["start"])
    {
        joystick["start"] = ps2x.Button(PSB_START);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_SELECT) == true)
    {
        joystick["ps2_select"] = ps2x.Button(PSB_SELECT);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_PAD_UP) == true)
    {
        joystick["pad_up"] = ps2x.Button(PSB_PAD_UP);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_PAD_DOWN) == true)
    {
        joystick["pad_down"] = ps2x.Button(PSB_PAD_DOWN);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_PAD_LEFT) == true)
    {
        joystick["pad_left"] = ps2x.Button(PSB_PAD_LEFT);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_PAD_RIGHT) == true)
    {
        joystick["pad_right"] = ps2x.Button(PSB_PAD_RIGHT);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_TRIANGLE) == true)
    {
        joystick["action_up"] = ps2x.Button(PSB_TRIANGLE);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_CROSS) == true)
    {
        joystick["action_down"] = ps2x.Button(PSB_CROSS);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_SQUARE) == true)
    {
        joystick["action_left"] = ps2x.Button(PSB_SQUARE);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_CIRCLE) == true)
    {
        joystick["action_right"] = ps2x.Button(PSB_CIRCLE);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_L1) == true)
    {
        joystick["shoulder_left"] = ps2x.Button(PSB_L1);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_R1) == true)
    {
        joystick["shoulder_right"] = ps2x.Button(PSB_R1);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_L2) == true)
    {
        joystick["trigger_left"] = ps2x.Button(PSB_L2);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_R2) == true)
    {
        joystick["trigger_right"] = ps2x.Button(PSB_R2);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_L3) == true)
    {
        joystick["left"] = ps2x.Button(PSB_L3);

        turnOnLEDtoShowMQTTMessage = true;
    }

    if (ps2x.Button(PSB_R3) == true)
    {
        joystick["right"] = ps2x.Button(PSB_R3);

        turnOnLEDtoShowMQTTMessage = true;
    }

    // joystick["xbox360_back"] = false;
    // joystick["xbox360_guide"] = false;
    // joystick["trigger_left_analog"] = 0;
    // joystick["trigger_right_analog"] = 0;
    //}

    JsonObject joystickObj = joystick.as<JsonObject>();

    // size_t joystickObjSize = joystickObj.size();
    // Serial.println(joystickObjSize);

    if (joystickObj.size() > 0)
    {
        joystick["dial"] = dial;
        joystick["make"] = "PS2";

        String json;

        serializeJson(joystick, json);

        //Serial.println(json);

        publishMQTTmessage(json);

        if (turnOnLEDtoShowMQTTMessage == true)
        {
            digitalWrite(LED_BUILTIN, LOW); //set LED to flash on
        }

        lastCommandSentMillis = millis(); //reset timer
    }
}
