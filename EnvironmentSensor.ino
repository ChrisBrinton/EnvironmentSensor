/***************************************************
  Adafruit MQTT Library ESP8266 Example

  Must use ESP8266 Arduino from:
    https://github.com/esp8266/Arduino

  Works great with Adafruit's Huzzah ESP board & Feather
  ----> https://www.adafruit.com/product/2471
  ----> https://www.adafruit.com/products/2821

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Tony DiCola for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <ESP8266WiFi.h>
#include <time.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "dht22.h"

//Since we're going to have multiple instances of this device, they all need a unique identifier
//Use this pound define to set it
#define UNIQUE_DEVICE_ID "MaineLRC1"
#define FIRMWARE_VERSION "prod_0_1"

// Data wire is plugged into pin 4 on the Arduino
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "Brinton5"
#define WLAN_PASS       "ClearAndPresentDanger"

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "cbrinton"
#define AIO_KEY         "e65860eb60684ac5b6890e8aeaff97ad"

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Store the MQTT server, username, and password in flash memory.
// This is required for using the Adafruit MQTT library.
const char MQTT_SERVER[] PROGMEM    = AIO_SERVER;
const char MQTT_USERNAME[] PROGMEM  = AIO_USERNAME;
const char MQTT_PASSWORD[] PROGMEM  = AIO_KEY;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, AIO_SERVERPORT, MQTT_USERNAME, MQTT_PASSWORD);

/****************************** Feeds ***************************************/

// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
const char INTTEMPSENSOR1_FEED[] PROGMEM = AIO_USERNAME "/feeds/IntTemp_" UNIQUE_DEVICE_ID;
const char EXTTEMPSENSOR1_FEED[] PROGMEM = AIO_USERNAME "/feeds/ExtTemp_" UNIQUE_DEVICE_ID;
const char HUMIDITYSENSOR1_FEED[] PROGMEM = AIO_USERNAME "/feeds/Humidity_" UNIQUE_DEVICE_ID;
const char DAILYHIGHINTTEMP_FEED[] PROGMEM = AIO_USERNAME "/feeds/IntTemp_DailyHigh_" UNIQUE_DEVICE_ID;
const char DAILYLOWINTTEMP_FEED[] PROGMEM = AIO_USERNAME "/feeds/IntTemp_DailyLow_" UNIQUE_DEVICE_ID;
const char DAILYHIGHEXTTEMP_FEED[] PROGMEM = AIO_USERNAME "/feeds/ExtTemp_DailyHigh_" UNIQUE_DEVICE_ID;
const char DAILYLOWEXTTEMP_FEED[] PROGMEM = AIO_USERNAME "/feeds/ExtTemp_DailyLow_" UNIQUE_DEVICE_ID;
Adafruit_MQTT_Publish IntTempSensor1 = Adafruit_MQTT_Publish(&mqtt, INTTEMPSENSOR1_FEED);
Adafruit_MQTT_Publish ExtTempSensor1 = Adafruit_MQTT_Publish(&mqtt, EXTTEMPSENSOR1_FEED);
Adafruit_MQTT_Publish HumiditySensor1 = Adafruit_MQTT_Publish(&mqtt, HUMIDITYSENSOR1_FEED);
Adafruit_MQTT_Publish DailyHighIntTemp = Adafruit_MQTT_Publish(&mqtt, DAILYHIGHINTTEMP_FEED);
Adafruit_MQTT_Publish DailyLowIntTemp = Adafruit_MQTT_Publish(&mqtt, DAILYLOWINTTEMP_FEED);
Adafruit_MQTT_Publish DailyHighExtTemp = Adafruit_MQTT_Publish(&mqtt, DAILYHIGHEXTTEMP_FEED);
Adafruit_MQTT_Publish DailyLowExtTemp = Adafruit_MQTT_Publish(&mqtt, DAILYLOWEXTTEMP_FEED);

// Setup a feed called 'onoff' for subscribing to changes.
const char ONOFF_FEED[] PROGMEM = AIO_USERNAME "/feeds/onoff";
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, ONOFF_FEED);

// Setup a subscription for getting unique DebugMsgs
const char DEBUGMSG_FEED[] PROGMEM = AIO_USERNAME "/feeds/DebugMsg_" UNIQUE_DEVICE_ID;
Adafruit_MQTT_Subscribe DebugMsgButton = Adafruit_MQTT_Subscribe(&mqtt, DEBUGMSG_FEED);

/*************************** Sketch Code ************************************/

#define HIGHTEMPBOUND 125
#define LOWTEMPBOUND -40
#define HIGHHUMIBOUND 100
#define LOWHUMIBOUND 0

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();
void DebugMsgRespond();

float Temp, internalTempF, externalTempF, DailyHighIntTempF, DailyLowIntTempF, DailyHighExtTempF, DailyLowExtTempF;
float Humi;
float readDelay;
int count, pingCount;
int prevHour = 0; //make sure we dont publish on startup
int currentHour;

time_t prevReadTime;

void setup() {
  Serial.begin(115200);
  delay(10);

  count = 0;
  pingCount = 0;

  //Set up the blinky
  pinMode(0, OUTPUT);
  
  Serial.print(F("Brinton Environment Sensor: "));
  Serial.println(UNIQUE_DEVICE_ID);
  Serial.print(F("Firmware Version: "));
  Serial.println(FIRMWARE_VERSION);

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  initDht();
  sensors.begin();

  initTime();

  DailyHighIntTempF = -100;
  DailyHighExtTempF = -100;
  DailyLowIntTempF = 150;
  DailyLowExtTempF = 150;
  readDelay = 950; // starting guess to create as close to a 1 sec loop as can be managed
  
  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&onoffbutton);
  
  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&DebugMsgButton);
}

void loop() {
  count++;
  int secSinceLastSample;
  if(count%2){
    digitalWrite(0, HIGH);
  }
  else{
    digitalWrite(0, LOW);
  }
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription((int)readDelay))) {
    if (subscription == &onoffbutton) {
      Serial.print(F("Got onoffbutton: "));
      Serial.println((char *)onoffbutton.lastread);
    }
    if (subscription == &DebugMsgButton) {
      Serial.print(F("Got DebugMsg request: "));
      Serial.println((char*)DebugMsgButton.lastread);
      if(strcmp((char*)DebugMsgButton.lastread,"1") == 0){
        DebugMsgRespond();
      }
    }
  }

  //Only get sensor data once every 10min. Since the Adafruit graph only accepts 1000 data points, this will give us one weeks worth of temp.
  //We run this loop every READ_DELAY ms, so that means that every 10*60/(READ_DELAY/1000) loops we publish a value

 if(count>=(10*60)) //publish every 10min
// if(count>=(60)) //publish every 1 min
 {
    struct tm *info = checkTime();
    readAndPublishSensors();    

    //Once a day publish the daily high and low temps. When the current hour < prev hour, the day has rolled over and we publish.
    if(info->tm_hour < prevHour){
      publishDailyValues();
      //Reset the daily values to something that should be overridden immediately
      DailyHighIntTempF = -100;
      DailyLowIntTempF = 150;
      DailyHighExtTempF = -100;
      DailyLowExtTempF = 150;
    }

    prevHour = info->tm_hour;
    count=0;
  }
  
  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  // We dont want to ping frequently because we could lose a message if we're pinging when a subscription message comes in
  // The loop should run about every second, so we'll ping 3 times more than we need to.
  if(pingCount > MQTT_CONN_KEEPALIVE/3){
    //Serial.println("Ping!");
    if(! mqtt.ping()) {
      mqtt.disconnect();
    }
    pingCount=0;
  }
  pingCount++;
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}

void initTime() {
   time_t epochTime;
   struct tm *info;
   char buffer[80];


    configTime(-4*3600, 1*3600, "pool.ntp.org", "time.nist.gov"); //The first parameter is the timezone offset from GMT in sec, the second is the DST offset in sec

    while (true) {
       epochTime = time(NULL);
       info = localtime( &epochTime );
    
        if (epochTime == 0) {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        } else {
            Serial.print("Fetched NTP epoch time is: ");
            Serial.println(epochTime);
            Serial.print("Current local time and date: ");
            Serial.println(asctime(info));
            prevReadTime = epochTime;
            prevHour = info->tm_hour;
            break;
        }
    }
}

struct tm * checkTime() {
  time_t readTime;
  struct tm *info;
  readTime = time(NULL);
  if(readTime==0){
      Serial.println("Fetching NTP epoch time failed!");
  } else {
      float readAdjust = 0;
      Serial.print("Fetched NTP epoch time is: ");
      Serial.println(readTime);
      info = localtime( &readTime );
      Serial.println(asctime(info));
      float timeDiff = readTime - prevReadTime;
      readAdjust = ((10*60) - timeDiff)/(10*60);
      Serial.print("old readDelay is ");
      Serial.println(readDelay);
      readDelay = readDelay*(1+readAdjust); //try to get the loop time as close to 1 sec as possible
      Serial.print("new readDelay is ");
      Serial.println(readDelay);
      prevReadTime = readTime;
  }
  return info;
}

void readAndPublishSensors(){
  // Get sensor data
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  Serial.print("Temperature for Device 1 is: ");
  externalTempF = sensors.getTempCByIndex(0)*1.8 + 32;
  Serial.println(externalTempF); // Why "byIndex"? You can have more than one IC on the same bus. 0 refers to the first IC on the wire   
  
  //There doesnt seem to be the concept of error handling on the device. We'll check for out of bounds temps and discard the sample.
  if(externalTempF < HIGHTEMPBOUND && externalTempF > LOWTEMPBOUND){
    Serial.print(F("\nSending ExternalTempSensor1 val "));
    Serial.print(externalTempF);
    Serial.print("...");
    if (! ExtTempSensor1.publish(externalTempF)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));
    }
  }
  else{
    Serial.print("Ext Temp out of bounds: ");
    Serial.println(externalTempF);
    Serial.println("Not publishing ExtTemp");
  }

  if(externalTempF > DailyHighExtTempF){
    DailyHighExtTempF = externalTempF;
  }

  if(externalTempF < DailyLowExtTempF){
    DailyLowExtTempF = externalTempF;
  }

  
  if(!getNextSample(&Temp, &Humi)){
    internalTempF = Temp*1.8 +32;
  
    if(internalTempF > DailyHighIntTempF){
      DailyHighIntTempF = internalTempF;
    }
  
    if(internalTempF < DailyLowIntTempF){
      DailyLowIntTempF = internalTempF;
    }
    
    if(internalTempF < HIGHTEMPBOUND && internalTempF > LOWTEMPBOUND){
      Serial.print(F("\nSending internalTempSensor1 val "));
      Serial.print(internalTempF);
      Serial.print("...");
      if (! IntTempSensor1.publish(internalTempF)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("OK!"));
      }
    }
    else{
      Serial.print("Ext Temp out of bounds: ");
      Serial.println(externalTempF);
      Serial.println("Not publishing ExtTemp");
    }
     if(Humi > LOWHUMIBOUND && Humi < HIGHHUMIBOUND){
       Serial.print(F("\nSending HumiditySensor1 val "));
       Serial.print(Humi);
       Serial.print("...");
       if (! HumiditySensor1.publish(Humi)) {
         Serial.println(F("Failed"));
       } else {
         Serial.println(F("OK!"));
       }
     }
     else{
       Serial.print("Humi out of bounds: ");
       Serial.println(Humi);
       Serial.println("Not publishing Humi");
     }
 
   }
   else {
    Serial.println("getNextSample returned fail!");
   }
}

void publishDailyValues() {
  Serial.print(F("\nSending DailyHighIntTemp val: "));
  Serial.print(DailyHighIntTempF);
  Serial.print("...");
  if(!DailyHighIntTemp.publish(DailyHighIntTempF)){
    Serial.println(F("Failed"));
  }
  else{
    Serial.println(F("OK!"));
  }
  Serial.print(F("\nSending DailyHighExtTemp val: "));
  Serial.print(DailyHighExtTempF);
  Serial.print("...");
  if(!DailyHighExtTemp.publish(DailyHighExtTempF)){
    Serial.println(F("Failed"));
  }
  else{
    Serial.println(F("OK!"));
  }  
  Serial.print(F("\nSending DailyLowIntTemp val: "));
  Serial.print(DailyLowIntTempF);
  Serial.print("...");
  if(!DailyLowIntTemp.publish(DailyLowIntTempF)){
    Serial.println(F("Failed"));
  }
  else{
    Serial.println(F("OK!"));
  }
  Serial.print(F("\nSending DailyLowIExtTemp val: "));
  Serial.print(DailyLowExtTempF);
  Serial.print("...");
  if(!DailyLowExtTemp.publish(DailyLowExtTempF)){
    Serial.println(F("Failed"));
  }
  else{
    Serial.println(F("OK!"));
  }

}

void DebugMsgRespond() {
  //When we get a debug message, we read and send current values for the sensors and daily high/low
  checkTime();
  readAndPublishSensors();
  publishDailyValues();
}

