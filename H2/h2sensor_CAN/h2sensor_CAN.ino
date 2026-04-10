// TB200B-ES1/ES4 Hydrogen Gas Sensor Module
// ESP32 (has 3 UARTs): UART2 used for H2 sensor, UART0 (USB) for debug output

#include <CAN.h>

// ----------PIN DEFINITIONS----------
#define SENSOR_RX     16   // GPIO16 = RX2 (data coming in from sensor)
#define SENSOR_TX     17   // GPIO17 = TX2 (data going out to sensor)

#define RELAY_PIN     13   // trips when concetration exceeds threshold
#define ALERT_LED     14   // ON when in ALERT state
#define RESET_BUTTON  22   // pressed by user to attempt reset
#define RESET_LED     21   // ON while system checks H2 levels during reset

#define TX_GPIO_NUM   32
#define RX_GPIO_NUM   33
#define CAN_ID        0x06

// ----------SENSOR DATA-------------
byte readIncoming[13];                                                    // 13-byte buffer stores data from sensor
byte command6[] = {0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78}; // 9-byte command sent to sensor to request reading

// Initialize values
float concentration_ppm = 0;
float temperature       = 0;
float humidity          = 0;

unsigned long last_read_time = 0;
const long interval = 500;

// --------THRESHOLD CONDITIONS------
float h2_threshold         = 150;  // H2 alert threshold in ppm (10000 ppm = 1% vol)
float temp_threshold    = 15.0;     // temperature threshold in deg C (unused for now?)

//----------DEFINE STATES------------
enum State {
  STATE_SAFE,                     // normal operation (below threshold)
  STATE_ALERT,                    // alert (exceeded threshold, trip relay)
  STATE_RESETTING                 // user pressed reset (check if concentration has dropped)
};

//initialize current state to safe
State current_state = STATE_SAFE;

// --------------------------------------------------------------------------------------------------------------------------

void send_on_CAN(void);

void setup() {
  Serial.begin(115200); // UART0: debug output to PC over USB
  Serial2.begin(9600, SERIAL_8N1, SENSOR_RX, SENSOR_TX); // UART2: H2 sensor communication must be at 9600 baud
  Serial2.setTimeout(200); // times out in 200ms if sensor stops responding

  // Set the pins
  CAN.setPins (RX_GPIO_NUM, TX_GPIO_NUM);

  // start the CAN bus at 500 kbps -- halved? 250kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }

  // Warm up the sensor 90s for stable readings (datasheet T90)
  Serial.println("Sensor warming up...");
  delay(3000);
  Serial.println("Sensor ready!");

  // Pin configurations
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(ALERT_LED, OUTPUT);
  pinMode(RESET_LED, OUTPUT);
  pinMode(RESET_BUTTON, INPUT);

  // Safe state defaults (relay ON, LEDs OFF)
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(ALERT_LED, HIGH);
  digitalWrite(RESET_LED, LOW);
  
  // Brief startup blink to confirm ESP32 boot
  delay(500);
  digitalWrite(ALERT_LED, LOW);

  Serial.println("TB200B H2 Sensor — ESP32 Ready");
}

// --------------------------------------------------------------------------------------------------------------------------
void read_sensor() {
  // Clear buffer
  memset(readIncoming, 0x00, sizeof(readIncoming));

  // Send command to sensor over UART2 (Serial2)
  Serial2.write(command6, sizeof(command6));
  delay(50); // give sensor time to react
  int bytesRead = Serial2.readBytes(readIncoming, sizeof(readIncoming));

  // -----------VALIDATION---------------------
  // Check if full 13-byte packet was received (timeout or unconnected sensor = less bytes)
  if (bytesRead < 13) {
    Serial.println("ERROR: Incomplete response from sensor");
    return;
  }

  // Check if first byte is 0xFF
  if (readIncoming[0] != 0xFF) {
    Serial.println("ERROR: Invalid response header");
    return;
  }
  
  // Verify checksum
  byte checksum = 0;
  for (int i = 1; i <= 11; i++) {
    checksum += readIncoming[i];
  }
  checksum = (~checksum) + 1; // two's complement: invert and add 1
  if (checksum != readIncoming[12]) {
    Serial.println("ERROR: Checksum mismatch");
    return;
  }

  // ------------CALCULATE VALUES---------------
  // convert H2 concentration to ppm
  concentration_ppm = ((readIncoming[6] * 256) + readIncoming[7]) * 10.0;

  // convert temperature to deg C (signed int allows conversion to work for both +ve and -ve values)
  temperature = ((int16_t)((readIncoming[8] << 8) | readIncoming[9])) / 100.0;
  
  // convert humidity to %RH
  humidity = ((int)((readIncoming[10] << 8) | readIncoming[11])) / 100.0;
    
  // ------------FINAL DEBUG OUTPUT-------------
  // can comment in or out these print statements to Serial Monitor
  Serial.printf("Concentration: %.2f ppm \n", concentration_ppm);
  Serial.printf("Temperature: %.2f degrees C \n", temperature);
  Serial.printf("Humidity: %.2f %%RH \n", humidity);
  Serial.println();
}

// ---------------------------------------------------------------------------------------------------------------------

void loop() {
  unsigned long current_time = millis();
  if(last_read_time + interval < current_time){
    last_read_time = current_time;
    read_sensor();
    send_on_CAN();
  }

  switch(current_state) {

    case STATE_SAFE:
      if (concentration_ppm >= h2_threshold) { // concentration over safety threshold -> UNSAFE!!
        Serial.println("   ALERT: Concentration above threshold");
        current_state = STATE_ALERT;        // change state to alert

        digitalWrite(RELAY_PIN, LOW);       // turn OFF relay
        digitalWrite(ALERT_LED, HIGH);      // turn ON alert light
      }
      if(digitalRead(RESET_BUTTON) == HIGH){
        Serial.println("   Reset initiated");
        current_state = STATE_RESETTING;
        digitalWrite(RESET_LED, HIGH);     
        delay(500);
        digitalWrite(RESET_LED, LOW); //brief blink
      }

      break;
    
    case STATE_ALERT:
      if (digitalRead(RESET_BUTTON) == HIGH) { // runs when the reset button is pressed
        Serial.println("   Reset button pressed — checking concentration...");
        current_state = STATE_RESETTING;

        digitalWrite(RESET_LED, HIGH);  //turn ON reset light (system thinking to detect concentration)
        delay(500);
        //brief pause
      }
      break;
    
    case STATE_RESETTING: 
      if (concentration_ppm < h2_threshold) { // concentration back under threshold -> safe
        Serial.println("   Concentration safe — returning to normal");
        current_state = STATE_SAFE;        // change state back to normal

        digitalWrite(RELAY_PIN, HIGH);     // turn ON relay (restore back to normal)
        digitalWrite(ALERT_LED, LOW);      // turn OFF alert light
      }
      else {  // concentration still above threshold -> UNSAFE!!
        Serial.println("   Concentration still high — back to ALERT");
        current_state = STATE_ALERT; // alert state
      }

      digitalWrite(RESET_LED, LOW);      // turn OFF reset light (system done detecting concentration)

      break;
  }
  }

void send_on_CAN(void){
    Serial.printf("Sending packet 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X ... ", 
      readIncoming[6], readIncoming[7], 
      readIncoming[8], readIncoming[9], 
      readIncoming[10], readIncoming[11]);
    
    CAN.beginPacket(CAN_ID);
    CAN.write(readIncoming[6]);
    CAN.write(readIncoming[7]);
    CAN.write(readIncoming[8]);
    CAN.write(readIncoming[9]);
    CAN.write(readIncoming[10]);
    CAN.write(readIncoming[11]);
    CAN.endPacket();

    Serial.println("done");
}