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
#include <String.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include "Adafruit_SSD1306.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "dht22.h"

//Since we're going to have multiple instances of this device, they all need a unique identifier
//Use this pound define to set it
#define UNIQUE_DEVICE_ID "TestLRC2"
#define FIRMWARE_VERSION "prod_0_2"

// Data wire is plugged into pin 4 on the Arduino
#define ONE_WIRE_BUS 4

// RGB and Blinky pins
#define BLINKYPIN 0

//Set up SPI for display

#define OLED_MOSI   5
#define OLED_CLK   14
#define OLED_DC    15
#define OLED_CS    12
#define OLED_RESET 13
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

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
  pinMode(BLINKYPIN, OUTPUT);

  //Set up the display
  display.begin(SSD1306_SWITCHCAPVCC);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.setTextWrap(FALSE);
  //display init done

  displayStartupLogo();

  Serial.print(F("GDT Environment Sensor: "));
  display.print("EnvSens:");
  Serial.println(UNIQUE_DEVICE_ID);
  display.println(UNIQUE_DEVICE_ID);
  display.display();
  scrollUpDB(1);
  Serial.print(F("Firmware Version: "));
  display.print("FVer: ");
  Serial.println(FIRMWARE_VERSION);
  display.println(FIRMWARE_VERSION);
  display.display();
  scrollUpDB(1);

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  display.print("Conn to ");
  Serial.println(WLAN_SSID);
  display.println(WLAN_SSID);
  display.display();
  scrollUpDB(1);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());
  display.print("IP addr: ");
  display.println(WiFi.localIP());
  display.display();
  scrollUpDB(1);
  delay(1000);
    
  initDht();
  sensors.begin();
  display.println("Sensors initialized");
  display.display();
  scrollUpDB(1);
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

  MQTT_connect();
  delay(2000);
  display.clearDisplay();
  scrollUpDB(-1);

  //Do an initial read of the sensors so the display has something to work with
  readSensors();
}

void loop() {
  count++;
  int secSinceLastSample;
  if(count%2){
    digitalWrite(BLINKYPIN, HIGH);
  }
  else{
    digitalWrite(BLINKYPIN, LOW);
  }

  displayInfo();

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
        debugMsgRespond();
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

  uint8_t retries = 5;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       display.println("MQTTconn fail, retry");
       display.display();
       scrollUpDB(1);
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
          display.println("MQTTconn fail, Abort");
          display.display();
          scrollUpDB(1);
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
  display.println("Cloud Connected");
  display.display();
  scrollUpDB(1);
}

void initTime() {
   time_t epochTime;
   struct tm *info;
   char buffer[80];
   String dateString,timeString;
   char linebuff[21];


    configTime(-4*3600, 1*3600, "pool.ntp.org", "time.nist.gov"); //The first parameter is the timezone offset from GMT in sec, the second is the DST offset in sec

    while (true) {
       epochTime = time(NULL);
       info = localtime( &epochTime );
    
        if (epochTime == 0) {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            display.println("NTP fail, retry in 2");
            display.display();
            scrollUpDB(1);
            delay(2000);
        } else {
            Serial.print("Fetched NTP epoch time is: ");
            Serial.println(epochTime);
            Serial.print("Current local time and date: ");
            Serial.println(asctime(info));
            display.println("NTP success");
            display.display();
            scrollUpDB(1);
            //strftime(linebuff,sizeof(linebuff),"%a %b %d %Y", info);
            dateString = String(info->tm_mon+1) + "/" + String(info->tm_mday) + "/" + String(info->tm_year+1900);
            display.println(dateString.c_str());
            display.display();
            scrollUpDB(1);
            //strftime(linebuff,sizeof(linebuff),"%r",info);
            timeString = String(info->tm_hour) + ":" + String(info->tm_min) + ":" + String(info->tm_sec);
            display.println(timeString.c_str());
            display.display();
            scrollUpDB(1);
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

  readSensors();
  
  if(externalTempF > DailyHighExtTempF){
    DailyHighExtTempF = externalTempF;
  }

  if(externalTempF < DailyLowExtTempF){
    DailyLowExtTempF = externalTempF;
  }

  
  if(internalTempF > DailyHighIntTempF){
    DailyHighIntTempF = internalTempF;
  }

  if(internalTempF < DailyLowIntTempF){
    DailyLowIntTempF = internalTempF;
  }
  
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

void readSensors() {
    // Get sensor data
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  Serial.print("Temperature for Device 1 is: ");
  externalTempF = sensors.getTempCByIndex(0)*1.8 + 32;
  Serial.println(externalTempF); // Why "byIndex"? You can have more than one IC on the same bus. 0 refers to the first IC on the wire   
  
  if(!getNextSample(&Temp, &Humi)){
    internalTempF = Temp*1.8 +32;
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

void debugMsgRespond() {
  //When we get a debug message, we read and send current values for the sensors and daily high/low
  checkTime();
  readAndPublishSensors();
  publishDailyValues();
}

void displayStartupLogo(){
  display.display();
  delay(2000);
  display.clearDisplay();
}

void scrollUp(int numoflines){
  static int line;
  for(int x=0;x<numoflines;x++){
    line++;
    if(line >= 4){
      for (int i=0;i<8;i++){
        delay(50);
        display.ssd1306_command(SSD1306_SETSTARTLINE+i);
      }
      //delay(3000);
      //after the row has scrolled up, copy the 2nd to 4th row into the 1st to 3rd row
      memcpy(display.getBuffer(),display.getBuffer()+128,384);
      //then blank out the 4th row
      memset(display.getBuffer()+384,0,128);
      //then set the start line back to the beginning of the buffer and move the cursor to the start of the new 4th line.
      display.ssd1306_command(SSD1306_SETSTARTLINE);
      display.setCursor(0,24);
      //now it should be safe to display the buffer
      display.display();
    }
  }
}

void scrollUpDB(int numoflines){
  static int line=0;
  static int bufferline=0;
  static int row=0;
  //reset counters. yay for procedural code! j/k
  if(numoflines==-1){
    line=0;
    bufferline=0;
    row=0;
    display.ssd1306_command(SSD1306_SETSTARTLINE);
    return;
  }
  for(int x=0;x<numoflines;x++){
    line++;
    bufferline++;
    if(line > 4){
      for (int i=0;i<8;i++){
        delay(50);
        display.ssd1306_command(SSD1306_SETSTARTLINE | row % 64);
        row++;
      }
      if(bufferline >= 8) {
        bufferline = 0;
        display.setCursor(0,0);
      }
      if(row >= 64){
        row = 0;
      }
    }
    //blank out the row that we just scrolled off
    if(bufferline < 5){
      memset(display.getBuffer()+((bufferline+3)*128),0,128);
    }
    else {
      memset(display.getBuffer()+((bufferline-5)*128),0,128);
    }
  }
}

//This should get called about every second. Cycle through the 4 actions
void displayInfo(){
  static int displayCount=0;
  static int upDown=0;

  displayCount++;
  if(displayCount==1){
    upDown++;
    String timeString = "";
    String ampmString = "";
    int hour;
    time_t readTime;
    struct tm *info;
    readTime = time(NULL);
    info = localtime( &readTime );
    hour = info->tm_hour;
    if(info->tm_hour>12){
      hour = info->tm_hour - 12;
    }
    timeString = timeString + String(hour) + ":";
    if(info->tm_min<10){
      timeString = timeString + "0";
    }
    timeString = timeString + String(info->tm_min);
    if(info->tm_hour>11){
      ampmString = "PM";
    }
    else{
      ampmString = "AM";
    }
    // do these shenanigans to determine the width of the time we want to print
    // so that we can center it on the screen
    int16_t x1,y1,centerx;
    uint16_t w,h;
    int total_w = 0;
    display.setFont(&FreeSans18pt7b);
    display.getTextBounds((char*)timeString.c_str(),0,31,&x1,&y1,&w,&h);
    total_w = w;
    display.setFont(&FreeSans9pt7b);
    display.getTextBounds((char*)ampmString.c_str(),0,31,&x1,&y1,&w,&h);
    total_w = total_w + w;

    centerx = (SSD1306_LCDWIDTH - total_w)/2;
    //on even, scroll down. on odd, scroll up
    if(upDown%2 == 0){
      display.setCursor(centerx,63);
      display.setFont(&FreeSans18pt7b);
      display.print(timeString.c_str());
      display.setFont(&FreeSans9pt7b);
      display.println(ampmString.c_str());
      display.display();
      for (int i=0;i<33;i++){
        display.ssd1306_command(SSD1306_SETSTARTLINE+i);
        delay(25);
      }
      //clear the area we just scrolled off for the next cycle
      memset(display.getBuffer(),0,512);    
    }
    else{
      display.setCursor(centerx,31);
      display.setFont(&FreeSans18pt7b);
      display.print(timeString.c_str());
      display.setFont(&FreeSans9pt7b);
      display.println(ampmString.c_str());
      display.display();
      for (int i=32;i>-1;i--){
        display.ssd1306_command(SSD1306_SETSTARTLINE+i);
        delay(25);
      }          
      memset(display.getBuffer()+512,0,512);
    }
  }
  else if(displayCount==3){
    upDown++;
    String tempString = "";
    int16_t x1,y1;
    uint16_t w,h;
    tempString = "In " + String((int)internalTempF);
    display.setFont(&FreeSans18pt7b);
    display.getTextBounds((char*)tempString.c_str(),0,31,&x1,&y1,&w,&h);
     if(upDown%2 == 0){
      display.setCursor((SSD1306_LCDWIDTH-w)/2,63);
      display.println(tempString.c_str());
      display.display();
      for (int i=0;i<33;i++){
        display.ssd1306_command(SSD1306_SETSTARTLINE+i);
        delay(25);
      }    
      memset(display.getBuffer(),0,512);    
    }
    else{
      display.setCursor((SSD1306_LCDWIDTH-w)/2,31);
      display.println(tempString.c_str());
      display.display();
      for (int i=32;i>-1;i--){
        display.ssd1306_command(SSD1306_SETSTARTLINE+i);
        delay(25);
      }          
      memset(display.getBuffer()+512,0,512);
    }
  }
  else if(displayCount==5){
    upDown++;
    String tempString = "";
    int16_t x1,y1;
    uint16_t w,h;
    tempString = "Out " + String((int)externalTempF);
    display.setFont(&FreeSans18pt7b);
    display.getTextBounds((char*)tempString.c_str(),0,31,&x1,&y1,&w,&h);
    if(upDown%2 == 0){
      display.setCursor((SSD1306_LCDWIDTH-w)/2,63);
    }
    else{
      display.setCursor((SSD1306_LCDWIDTH-w)/2,31);
    }
    display.println(tempString.c_str());
    display.display();
    if(upDown%2 == 0){
      for (int i=0;i<33;i++){
        display.ssd1306_command(SSD1306_SETSTARTLINE+i);
        delay(25);
      }    
      memset(display.getBuffer(),0,512);    
    }
    else{
      for (int i=32;i>-1;i--){
        display.ssd1306_command(SSD1306_SETSTARTLINE+i);
        delay(25);
      }          
      memset(display.getBuffer()+512,0,512);
    }
  }
  if(displayCount > 5){
    displayCount=0;
  }
  if(upDown>9){
    upDown=0;
  }
}

