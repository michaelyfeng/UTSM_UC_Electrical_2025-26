/* 
  JOULEMETER ESP32 Code
  V1 - oct 19 - modified original (removed screen & temp sensor)
*/


// Arduino Multi Function Energy Meter V1.0 
// Required Libraries
// https://github.com/adafruit/Adafruit_INA219
// https://github.com/adafruit/Adafruit_SSD1306
// Credit : GreatScott

#include <Wire.h>
#include <Adafruit_INA219.h>
#include <OneWire.h>

Adafruit_INA219 ina219;

float shuntvoltage = 0;
float busvoltage = 0;
float loadvoltage = 0;
float current_mA = 0;
float power_mW = 0;

unsigned long previousMillis = 0;
unsigned long interval = 1000;
float energy = 0;
float capacity=0;
float temp=0;

void setup() {
  // initialize ina219 with default measurement range of 32V, 2A
  Serial.begin(115200);
  Wire.begin(21,22);
  
  //ina219.begin();

  if (! ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }
  
  ina219.setCalibration_32V_2A();    // set measurement range to 32V, 2A  (do not exceed 26V!)
  // ina219.setCalibration_32V_1A();    // set measurement range to 32V, 1A  (do not exceed 26V!)
  // ina219.setCalibration_16V_400mA(); // set measurement range to 16V, 400mA

}
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis; 
    read_sensor_data();
    display_data();  
  }
}

void read_sensor_data(){  
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  if( current_mA <0)
  {
    current_mA=0;
  }
        //TEMP sensors.requestTemperatures(); // get temperatures
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  power_mW = loadvoltage*current_mA;  
  capacity = current_mA / 3600;
  energy = energy + loadvoltage * current_mA / 3600;
}
void display_data(){

  Serial.print("Bus Voltage:   "); 
  Serial.print(busvoltage); 
  Serial.println(" V");
  
  Serial.print("Current:       "); 
  Serial.print(current_mA); 
  Serial.println(" mA");

  Serial.print("Power:         "); 
  Serial.print(power_mW); 
  Serial.println(" mW");  

  Serial.print("Energy:         "); 
  Serial.print(energy); 
  Serial.println(" mWh");

  Serial.print("Capacity:         "); 
  Serial.print(capacity); 
  Serial.println(" mAh");

  Serial.println("");
}