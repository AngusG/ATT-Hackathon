#include "mbed.h" //not necessary for some reason?
#include "FXOS8700Q.h"
#include "LM75B.h"
//#include "MMA7660.h"
#include "MQTTClient.h"
#include "MQTTEthernet.h"
#include "C12832.h"
#include "Arial12x12.h"
#include "rtos.h"

// Update this to the next number *before* a commit
#define __APP_SW_REVISION__ "10"

//#define QUICKSTARTMODE 0
#define LED2_OFF 1

// Configuration values needed to connect to IBM IoT Cloud
#define ORG "xxspvo"             // For a registered connection, replace with your org
#define ID "EB14180084"          // For a registered connection, replace with your id
#define AUTH_TOKEN "H0sBz1WkwMRxPk8D!X"  // For a registered connection, replace with your auth-token
#define TYPE "iotsample-mbed-k64f"       // For a registered connection, replace with your type

#define MQTT_PORT 1883
#define MQTT_TLS_PORT 8883
#define IBM_IOT_PORT MQTT_PORT

#define MQTT_MAX_PACKET_SIZE 250

#if defined(TARGET_LPC1768)
#warning "Compiling for mbed LPC1768"
#include "LPC1768.h"

#elif defined(TARGET_K64F)
#warning "Compiling for mbed K64F"
#include "K64F.h"
#endif

bool quickstartMode = false;
char org[11] = ORG;  
char type[30] = TYPE;
char id[30] = ID;                 // mac without colons
char auth_token[30] = AUTH_TOKEN; // Auth_token is only used in non-quickstart mode

bool connected = false;
char* joystickPos = "CENTRE";
int blink_interval = 0;

/* Global Objects */

DigitalOut led_red(LED_RED);
DigitalOut led_green(LED_GREEN);
DigitalOut led_blue(LED_BLUE);

FXOS8700Q_acc acc( PTE25, PTE24, FXOS8700CQ_SLAVE_ADDR1);
FXOS8700Q_mag mag( PTE25, PTE24, FXOS8700CQ_SLAVE_ADDR1);
MotionSensorDataUnits acc_data;

Serial pc(USBTX, USBRX); //Terminal enable 

/* Declarations*/
int connect(MQTT::Client<MQTTEthernet, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTTEthernet* ipstack);
int getConnTimeout(int attemptNumber);
void attemptConnect(MQTT::Client<MQTTEthernet, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTTEthernet* ipstack);
int publish(MQTT::Client<MQTTEthernet, Countdown, MQTT_MAX_PACKET_SIZE>* client, MQTTEthernet* ipstack);
int getUUID48(char* buf, int buflen);
char* getMac(EthernetInterface& eth, char* buf, int buflen);
void messageArrived(MQTT::MessageData& md); 

void displayMessage(char* message);
void setMenu();
void printMenu(int menuItem);
void flashing_red(void const *args);  // to be used when the connection is lost
void flashing_yellow(void const *args);  
void green();
void off();
void red();
void yellow();