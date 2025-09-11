/* 
  JOULEMETER ESP32 Code
  V1 - sept 11 - modified original (removed screen & temp sensor)
*/


// Arduino Multi Function Energy Meter V1.0 
// Required Libraries
// https://github.com/adafruit/Adafruit_INA219
// https://github.com/adafruit/Adafruit_SSD1306
// Credit : GreatScott

#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

Adafruit_INA219 ina219;

      /*// Data wire is plugged into digital pin 2 on the Arduino
      #define ONE_WIRE_BUS 4
      // Setup a oneWire instance to communicate with any OneWire device
      OneWire oneWire(ONE_WIRE_BUS);  
      // Pass oneWire reference to DallasTemperature library
      DallasTemperature sensors(&oneWire);*/

float shuntvoltage = 0;
float busvoltage = 0;
float loadvoltage = 0;
float current_mA = 0;
float power_mW = 0;

unsigned long previousMillis = 0;
unsigned long interval = 100;
float energy = 0;
float capacity=0;
float temp=0;

void setup() {
  // initialize ina219 with default measurement range of 32V, 2A
  ina219.begin();
  Serial.begin(9600);
  ina219.setCalibration_32V_2A();    // set measurement range to 32V, 2A  (do not exceed 26V!)
  // ina219.setCalibration_32V_1A();    // set measurement range to 32V, 1A  (do not exceed 26V!)
  // ina219.setCalibration_16V_400mA(); // set measurement range to 16V, 400mA
  //sensors.begin();
      // initialize OLED display
      /*  TEMP display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(1);
      display.display();*/
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
        /*// show data on OLED
        display.clearDisplay();  
        display.setCursor(0, 0);
        display.print(busvoltage-0.08);
        display.print(" V");*/
  Serial.print("Bus Voltage:   "); 
  Serial.print(busvoltage); 
  Serial.println(" V");
  
        /*display.setCursor(0, 10);
        display.print(current_mA);
        display.print(" mA");*/
  Serial.print("Current:       "); 
  Serial.print(current_mA); 
  Serial.println(" mA");
        /*display.setCursor(0, 20);
        display.print(power_mW);
        display.print(" mW");*/
  Serial.print("Power:         "); 
  Serial.print(power_mW); 
  Serial.println(" mW");  
        /*display.setCursor(65,0);
        display.print(energy);
        display.println(" mWh");*/
  Serial.print("Energy:         "); 
  Serial.print(energy); 
  Serial.println(" mWh");
        /*display.setCursor(65,10);
        display.print(capacity);
        display.println(" mAh");*/
  Serial.print("Capacity:         "); 
  Serial.print(capacity); 
  Serial.println(" mAh");
        /*display.setCursor(65,20);
        display.print(sensors.getTempCByIndex(0));  
        display.println(" C");*/
        /*  TEMP Serial.print("Temperature:         "); 
        Serial.print(sensors.getTempCByIndex(0));
        Serial.print(" C");*/
  Serial.println("");
  display.display();
}
