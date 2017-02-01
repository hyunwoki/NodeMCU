#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_ADS1015.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <IRremoteESP8266.h>
#include "DHT.h"
#include "ap_setting.h"
#include "ADS1115.h"
#include "ir_code.h"
#include "icons.h"

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


Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 64)
  #error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

int  mqttReconnect = 1; //cnt
int  mqttLwtQos = 0;
int  mqttLwtRetain = true;
char message_buff[100];
String clientName;
unsigned long fast_update, slow_update;
unsigned long publish_cnt = 0;


WiFiClient wifiClient;
PubSubClient mqtt(mqtt_server, 1883, callback, wifiClient);


void wifi_connect() {

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  delay(200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    //WiFi.config(IPAddress(192, 168, 10, 112), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
  
    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Attempt++;

      if (EVENT_PRINT) {
        Serial.print(". ");
        Serial.print(Attempt); 
      }
           
      if (Attempt == 50)
      {
          if (EVENT_PRINT) {
              Serial.println();
              Serial.println("Could not connect to WIFI");
              ESP.restart();
          }
      }
    }
   if (EVENT_PRINT) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
   }
     digitalWrite(LED_GREEN, HIGH);
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
              display.clearDisplay();
              display.setTextSize(2);
              display.setTextColor(WHITE);
              display.setCursor(0,0);
              display.println("AC_POWER");
              display.println(" ");
              display.println("ON");
              display.display();
              delay(1000);
                    
            }
       else {                          
              irsend.sendRaw(turn_off, sizeof(turn_off)/sizeof(int), khz);    
              display.clearDisplay();
              display.setTextSize(2);
              display.setTextColor(WHITE);
              display.setCursor(0,0);
              display.println("AC_POWER");
              display.println(" ");
              display.println("OFF");
              display.display();    
              delay(1000);    
            }
     }      
   } 
}


void setup() {
  Serial.begin(115200);  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  delay(10);
  dht.begin();
  irsend.begin();    

  pinMode(LED_RED, OUTPUT);  
  pinMode(LED_GREEN, OUTPUT);  
  
  wifi_connect();    
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
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("SAMKANG M&T");
  display.println("www.sam-kang.com");
  display.display();

  ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 0.125mV
  ads.begin();
}

void loop() {

    if (WiFi.status() != WL_CONNECTED) {
        wifi_connect();
    }
    
    if (!mqtt.connected()) {
        reconnect();
    }

// Check for incoming data from the sensors  
    calcVI(20,200); 
    // serial_debug();
    OLED_display();
    Temperature();
    //Temperature_update();  


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
  display.clearDisplay();

  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
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
  display.clearDisplay();
  
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

