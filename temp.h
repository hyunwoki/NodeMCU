//long lastMsg = 0;
//float diff = 1.0;
//float temp = 0.0;
//float hum = 0.0;
//
//bool checkBound(float newValue, float prevValue, float maxDiff) 
//{return newValue < prevValue - maxDiff || newValue > prevValue + maxDiff;}
//
//void Temperature() {
//  
//  if ((millis()-fast_update)>5000) {
//        fast_update = millis();       
//        float newTemp = dht.readTemperature();
//        float newHum = dht.readHumidity();
//    
//        if (isnan(newTemp) || isnan(newHum)) {
//            Serial.println("Failed to read from DHT");    
//            LED_WARNING();
//            return;
//         }
//        
//        if (checkBound(newTemp, temp, diff)) {    
//            temp = newTemp;                 
//            mqtt.publish(mqttTopic"/TEMP", FtoS(temp).c_str(), true);      
//         }   
//             
//        if (checkBound(newHum, hum, diff)) {
//            hum = newHum;               
//            mqtt.publish(mqttTopic"/HUM", FtoS(hum).c_str(), true);       
//         }
//    }
//
//// 60sec update      
//      if ((millis()-slow_update)>10000) {
//           slow_update = millis();         
//           mqtt.publish(mqttTopic"/TEMP", FtoS(temp).c_str(), true);                     
//           mqtt.publish(mqttTopic"/HUM", FtoS(hum).c_str(), true);    
//      }
//}










//void Temperature(){
//
//// Wait a few seconds between measurements.
//  if ((millis()-slow_update)>5000) {
//      slow_update = millis();         
//
//  // Reading temperature or humidity takes about 250 milliseconds!
//  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
//  float h = dht.readHumidity();
//  // Read temperature as Celsius (the default)
//  float t = dht.readTemperature();
//  // Read temperature as Fahrenheit (isFahrenheit = true)
//  float f = dht.readTemperature(true);
//
//  // Check if any reads failed and exit early (to try again).
//  if (isnan(h) || isnan(t) || isnan(f)) {
//    Serial.println("Failed to read from DHT sensor!");
//    return;
//  }
//
//  // Compute heat index in Fahrenheit (the default)
//  float hif = dht.computeHeatIndex(f, h);
//  // Compute heat index in Celsius (isFahreheit = false)
//  float hic = dht.computeHeatIndex(t, h, false);
//
//
//// 60sec update      
//   if ((millis()-slow_update)>60000) {
//           slow_update = millis();         
//           mqtt.publish(mqttTopic"/TEMP", FtoS(t).c_str(), true);                     
//           mqtt.publish(mqttTopic"/HUM", FtoS(h).c_str(), true);    
//     }
//
//  if (DEBUG_PRINT){
//      Serial.print("Humidity: ");
//      Serial.print(h);
//      Serial.print(" %\t");
//      Serial.print("Temperature: ");
//      Serial.print(t);
//      Serial.print(" *C ");
//      Serial.print(f);
//      Serial.print(" *F\t");
//      Serial.print("Heat index: ");
//      Serial.print(hic);
//      Serial.print(" *C ");
//      Serial.print(hif);
//      Serial.println(" *F");
//      }
//   }
//}







