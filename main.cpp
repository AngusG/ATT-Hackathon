/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Sam Danbury - initial implementation
 *    Ian Craggs - refactoring to remove STL and other changes
 *    Sam Grove  - added check for Ethernet cable.
 *    Chris Styles - Added additional menu screen for software revision
 *    Angus Galloway - Fixed compatibility issues with FRDM-K64F
 *
 * To do :
 *    Add magnetometer sensor output to IoT data stream
 *
 *******************************************************************************/
 
#include "main.h"

int main()
{       
    int count = 0;
    pc.baud(115200);
    pc.printf("Main:\r\n");
    
    led_green = led_red = led_blue = 1; //all off
    
    quickstartMode = (strcmp(org, "quickstart") == 0);

    //lcd.set_font((unsigned char*) Arial12x12);  // Set a nice font for the LCD screen
    
    //led2 = LED2_OFF; //led_green = LED2_OFF; // K64F: turn off the main board LED 
    
    pc.printf("Connecting\r\n");
    //displayMessage("Connecting\n");
#if defined(TARGET_K64F)
    yellow();  // Don't flash on the K64F, because starting a thread causes the EthernetInterface init call to hang
#else
    //Thread yellow_thread(flashing_yellow);  
#endif
    
    MQTTEthernet ipstack;
    MQTT::Client<MQTTEthernet, Countdown, MQTT_MAX_PACKET_SIZE> client(ipstack);
    
    if (quickstartMode)
    {
#if defined(TARGET_K64F)
        getUUID48(id, sizeof(id));  // getMac doesn't work on the K64F
#else
        getMac(ipstack.getEth(), id, sizeof(id));
#endif
    }
    
    attemptConnect(&client, &ipstack);
    
    if (!quickstartMode) 
    {
        int rc = 0;
        if ((rc = client.subscribe("iot-2/cmd/+/fmt/json", MQTT::QOS1, messageArrived)) != 0)
            //WARN("rc from MQTT subscribe is %d\n", rc); 
            pc.printf("rc from MQTT subscribe is %d\n", rc); 
    }
    
    acc.enable(); //Enable accelerometer    
    blink_interval = 80;   
    
    while (true)
    {
        if (++count == 100)
        {               // Publish a message every second
            if (publish(&client, &ipstack) != 0) {
                attemptConnect(&client, &ipstack);   // if we have lost the connection
                pc.printf("Attempt connect\r\n");
            }
            count = 0;
        }
        
        if (blink_interval == 0)
            led_green = LED2_OFF; //led_green = LED2_OFF;
            
        else if (count % blink_interval == 0)
            //led2 = !led2; //led_green = !led_green;
            led_green = !led_green;
            
        /*if (count % 20 == 0)
            setMenu();*/
        client.yield(10);  // allow the MQTT client to receive messages
    }
}

/***************
 ** Functions **
 ****************/
int connect(MQTT::Client<MQTTEthernet, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTTEthernet* ipstack)
{   
    const char* iot_ibm = ".messaging.internetofthings.ibmcloud.com";
    
    char hostname[strlen(org) + strlen(iot_ibm) + 1];
    sprintf(hostname, "%s%s", org, iot_ibm);
    int rc = ipstack->connect(hostname, IBM_IOT_PORT);
    if (rc != 0)
        return rc;
     
    // Construct clientId - d:org:type:id
    char clientId[strlen(org) + strlen(type) + strlen(id) + 5];
    sprintf(clientId, "d:%s:%s:%s", org, type, id);
    DEBUG("clientid is %s\n", clientId);
    
    // MQTT Connect
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = clientId;
    
    if (!quickstartMode) 
    {        
        data.username.cstring = "use-token-auth";
        data.password.cstring = auth_token;
    }
    
    if ((rc = client->connect(data)) == 0) 
    {       
        connected = true;
        green();    
        //displayMessage("Connected");
        wait(2);
        //displayMessage("Scroll with joystick");
    }
    return rc;
}


int getConnTimeout(int attemptNumber)
{  // First 10 attempts try within 3 seconds, next 10 attempts retry after every 1 minute
   // after 20 attempts, retry every 10 minutes
    return (attemptNumber < 10) ? 3 : (attemptNumber < 20) ? 60 : 600;
}


void attemptConnect(MQTT::Client<MQTTEthernet, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTTEthernet* ipstack)
{
    int retryAttempt = 0;
    connected = false;
    
    // make sure a cable is connected before starting to connect
    while (!linkStatus()) {
        wait(1.0f);
        //WARN("Ethernet link not present. Check cable connection\n");
        pc.printf("Ethernet link not present. Check cable connection\r\n");
    }
        
    while (connect(client, ipstack) != 0) 
    {    
#if defined(TARGET_K64F)
        red();
#else
        Thread red_thread(flashing_red);
#endif
        int timeout = getConnTimeout(++retryAttempt);
        WARN("Retry attempt number %d waiting %d\n", retryAttempt, timeout);
        
        // if ipstack and client were on the heap we could deconstruct and goto a label where they are constructed
        //  or maybe just add the proper members to do this disconnect and call attemptConnect(...)
        
        // this works - reset the system when the retry count gets to a threshold
        if (retryAttempt == 5)
            NVIC_SystemReset();
        else
            wait(timeout);
    }
}


int publish(MQTT::Client<MQTTEthernet, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTTEthernet* ipstack)
{
    MQTT::Message message;
    char* pubTopic = "iot-2/evt/status/fmt/json";
            
    char buf[250];
    /*sprintf(buf,
     "{\"d\":{\"myName\":\"IoT mbed\",\"accelX\":%0.4f,\"accelY\":%0.4f,\"accelZ\":%0.4f,\"temp\":%0.4f,\"joystick\":\"%s\",\"potentiometer1\":%0.4f,\"potentiometer2\":%0.4f}}",
            MMA.x(), MMA.y(), MMA.z(), sensor.temp(), joystickPos, ain1.read(), ain2.read());*/
    
    acc.getAxis(acc_data);
    
    sprintf(buf,
     "{\"d\":{\"myName\":\"IoT mbed\",\"accelX\":%1.4f,\"accelY\":%1.4f,\"accelZ\":%1.4f,\"temp\":%0.4f,\"joystick\":\"%s\",\"potentiometer1\":%0.4f,\"potentiometer2\":%0.4f}}",
            acc_data.x, acc_data.y, acc_data.z, sensor.temp(), joystickPos, ain1.read(), ain2.read());        
            
            
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf);
    
    //pc.printf("Publishing %s\n", buf);
    //LOG("Publishing %s\n", buf);
    return client->publish(pubTopic, message);
}


#if defined(TARGET_K64F)
int getUUID48(char* buf, int buflen) 
{
    unsigned int UUID_LOC_WORD0 = 0x40048060;
    unsigned int UUID_LOC_WORD1 = 0x4004805C;
 
    // Fetch word 0
    uint32_t word0 = *(uint32_t *)UUID_LOC_WORD0;
 
    // Fetch word 1
    // we only want bottom 16 bits of word1 (MAC bits 32-47)
    // and bit 9 forced to 1, bit 8 forced to 0
    // Locally administered MAC, reduced conflicts
    // http://en.wikipedia.org/wiki/MAC_address
    uint32_t word1 = *(uint32_t *)UUID_LOC_WORD1;
    word1 |= 0x00000200;
    word1 &= 0x0000FEFF;
 
    int rc = snprintf(buf, buflen, "%4x%08x", word1, word0);   // Device id must be in lower case
    
    return rc;
}
#else
char* getMac(EthernetInterface& eth, char* buf, int buflen)    // Obtain MAC address
{   
    strncpy(buf, eth.getMACAddress(), buflen);

    char* pos;                                                 // Remove colons from mac address
    while ((pos = strchr(buf, ':')) != NULL)
        memmove(pos, pos + 1, strlen(pos) + 1);
    return buf;
}
#endif


void messageArrived(MQTT::MessageData& md)
{
    led_blue = 0;
    MQTT::Message &message = md.message;
    char topic[md.topicName.lenstring.len + 1];
    
    sprintf(topic, "%.*s", md.topicName.lenstring.len, md.topicName.lenstring.data);
    
    //LOG("Message arrived on topic %s: %.*s\n",  topic, message.payloadlen, message.payload);
    pc.printf("Message arrived on topic %s: %.*s\n",  topic, message.payloadlen, message.payload);
          
    // Command topic: iot-2/cmd/blink/fmt/json - cmd is the string between cmd/ and /fmt/
    char* start = strstr(topic, "/cmd/") + 5;
    int len = strstr(topic, "/fmt/") - start;
    
    if (memcmp(start, "blink", len) == 0)
    {
        char payload[message.payloadlen + 1];
        sprintf(payload, "%.*s", message.payloadlen, (char*)message.payload);
    
        char* pos = strchr(payload, '}');
        if (pos != NULL)
        {
            *pos = '\0';
            if ((pos = strchr(payload, ':')) != NULL)
            {
                int blink_rate = atoi(pos + 1);       
                blink_interval = (blink_rate <= 0) ? 0 : (blink_rate > 50 ? 1 : 50/blink_rate);
            }
        }
    }
    else
        pc.printf("Unsupported command: %.*s\n", len, start);
        //WARN("Unsupported command: %.*s\n", len, start);
}

void off()
{
    r = g = b = 1.0;    // 1 is off, 0 is full brightness
}

void red()
{
    led_red = 0.7; //r = 0.7; g = 1.0; b = 1.0;    // 1 is off, 0 is full brightness
}

void yellow()
{
    led_red = 0.7;
    led_green = 0.7; 
    led_blue = 1; // r = 0.7; g = 0.7; b = 1.0;    // 1 is off, 0 is full brightness
}

void green()
{
    led_red = 1;
    led_green = 0.7; 
    led_blue = 1; // r = 0.7; g = 0.7; b = 1.0;    // 1 is off, 0 is full brightness
}


void flashing_yellow(void const *args)
{
    bool on = false;
    while (!connected)    // flashing yellow only while connecting 
    {
        on = !on; 
        if (on)
            yellow();
        else
            off();   
        wait(0.5);
    }
}


void flashing_red(void const *args)  // to be used when the connection is lost
{
    bool on = false;
    while (!connected)
    {
        on = !on;
        if (on)
            red();
        else
            off();
        wait(2.0);
    }
}


void printMenu(int menuItem) 
{
    lcd.cls();
    lcd.locate(0,0);
    switch (menuItem)
    {
        case 0:
            lcd.printf("IBM IoT Cloud");
            lcd.locate(0,16);
            lcd.printf("Scroll with joystick");
            break;
        case 1:
            lcd.printf("Go to:");
            lcd.locate(0,16);
            lcd.printf("http://ibm.biz/iotqstart");
            break;
        case 2:
            lcd.printf("Device Identity:");
            lcd.locate(0,16);
            lcd.printf("%s", id);
            break;
        case 3:
            lcd.printf("Status:");
            lcd.locate(0,16);
            lcd.printf(connected ? "Connected" : "Disconnected");
            break;
        case 4:
            lcd.printf("App version:");
            lcd.locate(0,16);
            lcd.printf("%s",__APP_SW_REVISION__);
            break;
    }
}


void setMenu()
{
    static int menuItem = 0;
    if (Down)
    {
        joystickPos = "DOWN";
        if (menuItem >= 0 && menuItem < 4)
            printMenu(++menuItem);
    } 
    else if (Left)
        joystickPos = "LEFT";
    else if (Click)
        joystickPos = "CLICK";
    else if (Up)
    {
        joystickPos = "UP";
        if (menuItem <= 4 && menuItem > 0)
            printMenu(--menuItem);
    }
    else if (Right)
        joystickPos = "RIGHT";
    else
        joystickPos = "CENTRE";
}


/**
 * Display a message on the LCD screen prefixed with IBM IoT Cloud
 */
void displayMessage(char* message)
{
    lcd.cls();
    lcd.locate(0,0);        
    lcd.printf("IBM IoT Cloud");
    lcd.locate(0,16);
    lcd.printf(message);
}
