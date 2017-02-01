

#define mqttClientNode        "PowerMeter"
#define mqttClientId          "TEST"
#define mqtt_server           "211.180.34.28"
#define mqttDomain            "SKMT"
#define mqttTopic             mqttDomain"/"mqttClientId
#define mqttLwtTopic          mqttDomain"/"mqttClientId"/LWT"
#define humidity_topic        "sensor/humidity"
#define temperature_topic     "sensor/temperature"
//#define mqttUsername        "samknag"
//#define mqttPassword        "test1234"


const char* server = "api.thingspeak.com";
//WiFiClient client;

// replace with your channel's thingspeak API key, 
String apiKey = "8JIOUYKF34WE5SZ0";


// WIFI SETTING
const char* ssid     = "SAMKANG_IoT";
const char* password = "test1234";
//const char* ssid     = "SKMT";
//const char* password = "20222023";
