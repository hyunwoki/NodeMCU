#include <DHT.h>
#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_ADS1015.h>
#include <IRremoteESP8266.h>
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
#include "ap_setting.h"
#include "ADS1115.h"
#include "ir_code.h"


//DEBUG PRINT
#define DEBUG_PRINT 1
#define EVENT_PRINT 1
#define PAYLOAD_PRINT 1


// DHT22
#define DHTPIN    14
#define DHTTYPE   DHT22     
DHT dht(DHTPIN, DHTTYPE);


//LED STATUS
#define LED_RED   13
#define LED_GREEN 15


//IR RECIVER PIN
#define IR_IN     15
#define IR_OUT    12


// IR_SENSOR
int khz = 38;
IRrecv irrecv(IR_IN);
IRsend irsend(IR_OUT);


long lastMsg = 0;
float diff = 1.0;
float temp = 0.0;
float hum = 0.0;
int power = 0.0;

int loop_cnt = 0;
int  mqttReconnect = 0; 
int  mqttLwtQos = 0;
int  mqttLwtRetain = true;
char message_buff[100];
String clientName;

String AC_state;
String AS_state;

unsigned long fast_update, slow_update, loop_update;
unsigned long fast_update2, slow_update2;
unsigned long publish_cnt = 0;


WiFiClient wifiClient;
PubSubClient mqtt(mqtt_server, 1883, callback, wifiClient);



/***************************
 * Begin Settings
 **************************/
 

// Setup
//const int UPDATE_INTERVAL_SECS = 10 * 60; // Update every 10 minutes
const int UPDATE_INTERVAL_SECS = 5*60; // Update every 10 minutes

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
void drawEnergy(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawTemp(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawInfo(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
//FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast, drawThingspeak, drawEnergy1, drawEnergy2 };

FrameCallback frames[] = {drawDateTime, drawEnergy, drawTemp, drawInfo};
int numberOfFrames = 4;

OverlayCallback overlays[] = { drawHeaderOverlay};
int numberOfOverlays = 1;


void setup() {

  Serial.begin(115200);  
  Serial.println();
  Serial.println();

  dht.begin();
  
  // initialize dispaly
  display.init();
  display.clear();
  display.display();


  //display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);


  // wifi connection
  wifi_connect();

  // initialize
  irsend.begin();


  // OUTPUT initialize
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

  ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 0.125mV
  ads.begin();
}


void wifi_connect() {

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);  
  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 2, "Connecting to WiFi");
    display.drawXbm(34, 18, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);    
    display.drawXbm(46, 56, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 56, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 56, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();
    counter++;
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  LED_ONLINE();
  
  ui.setTargetFPS(15);
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
      //mqtt.publish(mqttLwtTopic, data, 1, mqttLwtRetain);
      mqtt.publish(mqttLwtTopic, "1", true);
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
    for (i = 0; i < length; i++) {
      message_buff[i] = payload[i];
    }
    message_buff[i] = '\0';
    String msgString = String(message_buff);
    Serial.println("Inbound: " + String(topic) + ":" + msgString);
    LED_PAYLOAD();

    if (String(topic) == mqttTopic"/IR/POWER") {
      if (msgString == "1") {
        irsend.sendRaw(turn_on, sizeof(turn_on) / sizeof(int), khz);
        AC_state = "ON";
      }
      else {
        irsend.sendRaw(turn_off, sizeof(turn_off) / sizeof(int), khz);
        AC_state = "OFF";
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
          mqttReconnect++;   
        }
      
  int remainingTimeBudget = ui.update();
  if ((millis()-loop_update)>1000) {
      loop_update = millis();      
          if (remainingTimeBudget > 0) {
                calcVI(10, 200);
                Energy_upload();  
                Temperature();  
                mqtt.loop();       
                loop_cnt++;  
                delay(remainingTimeBudget);        
                }        
    }

  if(loop_cnt == 1000){
     loop_cnt = 0;
  }       
}


void Temperature(){

// Wait a few seconds between measurements.
  if ((millis()-fast_update)>5000) {
      fast_update = millis();         

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

   temp = t, hum = h;

   
// 60sec update      
   if ((millis()-slow_update)>60000) {
           slow_update = millis();         
           mqtt.publish(mqttTopic"/TEMP", FtoS(t).c_str(), true);                     
           mqtt.publish(mqttTopic"/HUM", FtoS(h).c_str(), true);    
     }

  if (DEBUG_PRINT){
      Serial.print("Humidity: ");
      Serial.print(h);
      Serial.print(" %\t");
      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.print(" *C ");
      Serial.print(f);
      Serial.print(" *F\t");
      Serial.print("Heat index: ");
      Serial.print(hic);
      Serial.print(" *C ");
      Serial.print(hif);
      Serial.println(" *F");
      }
   }
}

void Energy_upload(){

    if ((millis()-fast_update2)>60000) {
          fast_update2 = millis();         
          power = Irms1 * 230;
          dtostrf (power, 3, 1, message_buff);
          mqtt.publish(mqttTopic"/POWER1", message_buff);  
          delay(50);
          power = Irms2 * 230;
          dtostrf (power, 3, 1, message_buff);
          mqtt.publish(mqttTopic"/POWER2", message_buff);  
    }    
}

void serial_debug() {
    if (DEBUG_PRINT) {
      Serial.print("Irms1:");
      Serial.println(Irms1);
      Serial.print("Irms2:");
      Serial.println(Irms2);
      Serial.print("Vrms:");
      Serial.println(Vrms);
      Serial.print("Power1:");
      Serial.println(Irms1 * 230);
      Serial.print("Power2:");
      Serial.println(Irms2 * 230);
  }
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
  drawProgress(display, 80, "Updating Temperature...");
  thingspeak.getLastChannelItem(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_READ_KEY);
  lastUpdate = timeClient.getFormattedTime();
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(500);
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

void drawEnergy(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(30 + x, 10 + y, "Watt1");
  display->setFont(ArialMT_Plain_16);
  power = Irms1 * 230;
  String strValue1 = String(power);
  display->drawString(33 + x, 23 + y, strValue1 + "w");

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(105 + x, 10 + y, "Watt2");
  display->setFont(ArialMT_Plain_16);
  power = Irms2 * 230;
  String strValue2 = String(power);
  display->drawString(103 + x, 23 + y, strValue2 + "w");
}

void drawTemp(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);  
  
  display->drawString(22 + x, 10 + y, "Temp");
  display->setFont(ArialMT_Plain_16);
  dtostrf (temp, 3, 1, message_buff);
  String temperature = String(message_buff) + "°c";
  display->drawString(15 + x, 23 + y, temperature);
  display->setFont(ArialMT_Plain_10);
   
  display->drawString(75 + x, 10 + y, "Humidity");
  display->setFont(ArialMT_Plain_16);
  dtostrf (hum, 3, 1, message_buff);
  String humidity = String(message_buff)  + "%";
  display->drawString(75 + x, 23 + y, humidity);
}

void drawInfo(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0 + y, "WIFI INFO");
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);  
  display->drawString(64 + x, 10 + y, "SSID:");
  display->drawString(64 + x, 20 + y, WIFI_SSID);
  display->drawString(64 + x, 30 + y, "DEVICE ID:");
  String ID = String(mqttClientId);    
  display->drawString(64 + x, 40 + y, ID);

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
  display->setTextAlignment(TEXT_ALIGN_LEFT);  
  display->drawString(0, 54, time);
  display->drawString(0, 40, AC_state);
  String cnt1 = String(loop_cnt);
  display->drawString(0, 0, cnt1);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);  
  String cnt2 = String(mqttReconnect);
  display->drawString(128, 0, cnt2);


  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  dtostrf (temp, 3, 1, message_buff);
  String temperature = String(message_buff) + "°c";
  display->drawString(128, 54, temperature);

  dtostrf (hum, 3, 1, message_buff);
  String humidity = String(message_buff) + "%";
  display->drawString(128, 40, humidity);
  display->drawHorizontalLine(0, 53, 128);  
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, wunderground.getWeatherText());

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

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

void LED_PAYLOAD() {
  digitalWrite(LED_GREEN, HIGH); delay(20); digitalWrite(LED_GREEN, LOW);   
}

void LED_ONLINE() {
  digitalWrite(LED_GREEN, HIGH);  
}

void LED_WIFI() {
  digitalWrite(LED_GREEN, HIGH);  delay(20);  digitalWrite(LED_GREEN, LOW);  
}

void LED_WARNING() {
  digitalWrite(LED_RED, HIGH); delay(100); digitalWrite(LED_RED, LOW);   
}

void LED_ERR() {
  digitalWrite(LED_RED, HIGH); 
}

// float형 숫자, 소수부분이 0인경우 정수로, 소수부분이 0이 아닌 숫자라면 소수점 이하 첫째자리까지만 표시
  static String FtoS(float num) {
    String str = (String) num;
      if ((int) num == num) {
       return str.substring(0, str.indexOf(".") + 2);
       } else {
       return str.substring(0, str.indexOf(".") + 2);
      }
  }

String macToStr(const uint8_t* mac){
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

