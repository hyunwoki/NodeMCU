#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_ADS1015.h>

#include <Ticker.h>
#include <JsonListener.h>
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "WundergroundClient.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "TimeClient.h"
#include "ThingspeakClient.h"

#include <IRremoteESP8266.h>
#include "DHT.h"
#include "ap_setting.h"
#include "ADS1115.h"
#include "ir_code.h"
//#include "icons.h"



//DEBUG PRINT
#define DEBUG_PRINT 1
#define EVENT_PRINT 1
#define PAYLOAD_PRINT 1


// DHT22
#define DHT_PIN   12
DHT dht = DHT(DHT_PIN, DHT22);


//LED STATUS
#define LED_RED   13 
#define LED_GREEN 15
#define OLED_RESET LED_BUILTIN  //4


//IR RECIVER PIN
#define IR_IN     15
#define IR_OUT    14


// IR_SENSOR
int khz = 38;
IRrecv irrecv(IR_IN);
IRsend irsend(IR_OUT);


float value;
int  mqttReconnect = 1; //cnt
int  mqttLwtQos = 0;
int  mqttLwtRetain = true;
char message_buff[100];
String clientName;
unsigned long fast_update, slow_update;
unsigned long publish_cnt = 0;


WiFiClient wifiClient;
PubSubClient mqtt(mqtt_server, 1883, callback, wifiClient);




/***************************
 * Begin Settings
 **************************/
// Please read http://blog.squix.org/weatherstation-getting-code-adapting-it
// for setup instructions

// WIFI
const char* WIFI_SSID = "SKMT";
const char* WIFI_PWD = "20222023";


// Setup
const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 10 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D2;
const int SDC_PIN = D1;

// TimeClient settings
const float UTC_OFFSET = 9;

// Wunderground Settings
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "15cfc1c435d18159";
const String WUNDERGRROUND_LANGUAGE = "KR";
const String WUNDERGROUND_COUNTRY = "KO";
const String WUNDERGROUND_CITY = "Cholwon";

//Thingspeak Settings
const String THINGSPEAK_CHANNEL_ID = "67284";
const String THINGSPEAK_API_READ_KEY = "L2VIW20QVNZJBLAK";

// Initialize the oled display for address 0x3c
// sda-pin=14 and sdc-pin=12
SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi   ui( &display );

/***************************
 * End Settings
 **************************/

TimeClient timeClient(UTC_OFFSET);

// Set to false, if you prefere imperial/inches, Fahrenheit
WundergroundClient wunderground(IS_METRIC);

ThingspeakClient thingspeak;

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

Ticker ticker;

//declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawThingspeak(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawEnergy1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawEnergy2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
//FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast, drawThingspeak, drawEnergy1, drawEnergy2 };

FrameCallback frames[] = { drawDateTime, drawEnergy1, drawEnergy2 };
int numberOfFrames = 3;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;





void setup() {
  
  Serial.begin(115200);  
  Serial.println();
  Serial.println();

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  
  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);  
  
  
  //display.begin(SSD1306_SWITCHCAPVCC, 0x3C);


  // wifi connection
  wifi_connect();    

  // initialize 
  dht.begin();
  irsend.begin();    

  pinMode(LED_RED, OUTPUT);  
  pinMode(LED_GREEN, OUTPUT);  
  
  
  mqtt.setServer(mqtt_server, 1883);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  clientName += mqttClientId;
  clientName += ".";  
  clientName += mqttClientNode;
  clientName += "-";
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);


  if (DEBUG_PRINT) {
      Serial.begin (115200);
      Serial.println();
      Serial.println("Energry controller Starting");
      Serial.print("ESP.getChipId() : ");
      Serial.println(ESP.getChipId());
      Serial.print("ESP.getFlashChipId() : ");
      Serial.println(ESP.getFlashChipId());
      Serial.print("ESP.getFlashChipSize() : ");
      Serial.println(ESP.getFlashChipSize());
  }    

   if (DEBUG_PRINT) {
      Serial.print("Client : ");
      Serial.println(mqttClientId);      
      Serial.print("NODE : ");
      Serial.println(mqttClientNode);      
      Serial.print("Connecting to ");
      Serial.print(mqtt_server);      
      Serial.print(" as ");
      Serial.println(clientName);
    }
  
  // Clear the buffer.
  display.display();
  display.println("SAMKANG M&T");
  display.println("www.sam-kang.com");
  display.display();

  ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 0.125mV
  ads.begin();
}





void wifi_connect() {

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(WIFI_SSID, WIFI_PWD);
   
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();
    counter++;
  }       
    
          
  ui.setTargetFPS(30);
  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();

  Serial.println("");

  updateData(&display);

  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);

}  


void reconnect() {
  while (!mqtt.connected()) {
     if (EVENT_PRINT) { 
         Serial.print("Attempting MQTT connection...");
     }
       if (mqtt.connect((char*) clientName.c_str(), mqttLwtTopic, mqttLwtQos, mqttLwtRetain, "0")) {
            byte data[] = { '1' };
            mqtt.subscribe(mqttTopic"/#");    
            mqtt.publish(mqttLwtTopic, data, 1, mqttLwtRetain);     
            //mqtt.publish(mqttLwtTopic, "1", true);     
            //mqtt.publish(mqttTopic"/ID", mqttClientId);  
               if (EVENT_PRINT) {           
                  Serial.println("connected");    
                  mqttReconnect += mqttReconnect;
               }
        } else {
             if (EVENT_PRINT) {
                Serial.print("failed, rc=");
                Serial.print(mqtt.state());
                Serial.println(" try again in 1 seconds");                
             }
            LED_ERR();
      }
  }
}


void callback(char* topic, byte* payload, unsigned int length) {
   if (PAYLOAD_PRINT) {
      int i = 0;
      for(i=0; i<length; i++) {
      message_buff[i] = payload[i];     
      }      
      message_buff[i] = '\0';
      String msgString = String(message_buff);      
      Serial.println("Inbound: " + String(topic) +":"+ msgString);  
      LED_PAYLOAD();

     if (String(topic) == mqttTopic"/IR/POWER") {
       if (msgString == "1") {                         
              irsend.sendRaw(turn_on, sizeof(turn_on)/sizeof(int), khz);    
//              display.clearDisplay();
//              display.setTextSize(2);
//              display.setTextColor(WHITE);
//              display.setCursor(0,0);
              display.println("AC_POWER");
              display.println(" ");
              display.println("ON");
              display.display();
              delay(1000);
                    
            }
       else {                          
              irsend.sendRaw(turn_off, sizeof(turn_off)/sizeof(int), khz);    
//              display.clearDisplay();
//              display.setTextSize(2);
//              display.setTextColor(WHITE);
//              display.setCursor(0,0);
              display.println("AC_POWER");
              display.println(" ");
              display.println("OFF");
              display.display();    
              delay(1000);    
            }
     }      
   } 
}


void loop() {

    if (WiFi.status() != WL_CONNECTED) {
        wifi_connect();
    }
    
    if (!mqtt.connected()) {
        reconnect();
    }


  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }





// Check for incoming data from the sensors  
    //calcVI(20,200); 
    //serial_debug();
    //OLED_display();
    //Temperature();
    //Temperature_update();  
    //mqtt.publish(mqttTopic"/POWER1", message_buff);

// mqtt 10sec connect keep     
   mqtt.loop();
}



void serial_debug(){
  if (DEBUG_PRINT){
      //Serial.print("Real Power:");
      //Serial.println(realPower);
      Serial.print("Irms1:");
      Serial.println(Irms);     
      Serial.print("Irms2:");
      Serial.println(IrmsII);   
      Serial.print("Vrms:");
      Serial.println(Vrms);   
      Serial.print("Power1:");
      Serial.println(Irms*230);
      Serial.print("Power2:");
      Serial.println(IrmsII*230);
  }
}


void OLED_display(){

   // Clear the buffer.
//  display.clearDisplay();

  // text display tests
//  display.setTextSize(1);
//  display.setTextColor(WHITE);
//  display.setCursor(0,0);
  display.println("Energy Monitoring");
  display.println(" ");
  
  display.print("POWER1: ");
  display.print(Irms*230);   
  display.println(" W");

  display.print("POWER2: ");
  display.print(IrmsII*230); 
  display.println(" W");

  display.print("Vrms: ");
  display.print(Vrms); 
  display.println(" V");
  
  count = count+1;
  display.println(" ");
  display.print("Loop Count: ");
  display.println(count);
  display.print("Mqtt Count: ");
  display.println(mqttReconnect);

  display.display();
  delay(1000);
//  display.clearDisplay();
  
}


void LED_RUN(){
  digitalWrite(LED_GREEN, HIGH);  delay(250);
  digitalWrite(LED_GREEN, LOW);   delay(250);
}


void LED_WIFI(){
  digitalWrite(LED_GREEN, HIGH);  delay(250);
  digitalWrite(LED_GREEN, LOW);   delay(250);
}


void LED_PAYLOAD(){
  digitalWrite(LED_RED, HIGH);  delay(100);
  digitalWrite(LED_RED, LOW);   delay(100);
}

void LED_ERR(){
  digitalWrite(LED_RED, HIGH);  delay(250);
  digitalWrite(LED_RED, LOW);   delay(250);
}



String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}




void thingspeak_connect(){

  if (wifiClient.connect(server,80)) {  //   "184.106.153.149" or api.thingspeak.com
    String postStr = apiKey;
           postStr +="&field1=";
           postStr += String(realPower);
           postStr +="&field2=";
           postStr += String(Vrms);
           postStr +="&field3=";
           postStr += String(Irms);
           postStr +="&field4=";
           postStr += String(powerFactor);
           postStr += "\r\n\r\n";
 
     wifiClient.print("POST /update HTTP/1.1\n"); 
     wifiClient.print("Host: api.thingspeak.com\n"); 
     wifiClient.print("Connection: close\n"); 
     wifiClient.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n"); 
     wifiClient.print("Content-Type: application/x-www-form-urlencoded\n"); 
     wifiClient.print("Content-Length: "); 
     wifiClient.print(postStr.length()); 
     wifiClient.print("\n\n"); 
     wifiClient.print(postStr);  
  }
  wifiClient.stop();

  Serial.println("Waiting...");    
  // thingspeak needs minimum 15 sec delay between updates
  //delay(5000);  
  
}



void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Updating time...");
  timeClient.updateTime();
  drawProgress(display, 30, "Updating conditions...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 50, "Updating forecasts...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 80, "Updating thingspeak...");
  thingspeak.getLastChannelItem(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_READ_KEY);
  lastUpdate = timeClient.getFormattedTime();
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}



void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = wunderground.getDate();
  int textWidth = display->getStringWidth(date);
  display->drawString(64 + x, 5 + y, date);
  display->setFont(ArialMT_Plain_24);
  String time = timeClient.getFormattedTime();
  textWidth = display->getStringWidth(time);
  display->drawString(64 + x, 15 + y, time);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  //display->setTextAlignment(TEXT_ALIGN_LEFT);
  //display->drawString(60 + x, 5 + y, wunderground.getWeatherText());

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(60 + x, 15 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
}


void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 2);
  drawForecastDetails(display, x + 88, y, 4);
}

void drawThingspeak(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0 + y, "Outdoor");
  display->setFont(ArialMT_Plain_16);
  display->drawString(64 + x, 10 + y, thingspeak.getFieldValue(0) + "°C");
  display->drawString(64 + x, 30 + y, thingspeak.getFieldValue(1) + "%");
}



void drawEnergy1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0 + y, "POWER1");
  
  display->setFont(ArialMT_Plain_24);
  //calcVI(20,200); 
  value = Irms*230;
  dtostrf (value, 3, 1, message_buff);
  String strValue = String(message_buff);    
  display->drawString(64 + x, 15 + y, strValue + "W");
  
  //display->drawString(64 + x, 20 + y, strValue);
  //display->drawString(64 + x, 30 + y, thingspeak.getFieldValue(1) + "V");
}


void drawEnergy2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0 + y, "POWER2");
  display->setFont(ArialMT_Plain_16);
  display->drawString(64 + x, 10 + y, thingspeak.getFieldValue(0) + "W");
  display->drawString(64 + x, 30 + y, thingspeak.getFieldValue(1) + "V");
}







void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->drawString(x + 20, y, day);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, wunderground.getForecastIcon(dayIndex));

  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  String time = timeClient.getFormattedTime().substring(0, 5);
  //display->setTextAlignment(TEXT_ALIGN_LEFT);
  //display->drawString(0, 54, time);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}



