//byte readIncoming_8[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//place holders for 12 bytes of data
byte readIncoming_12[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//byte readIncoming_4[] = {0x00, 0x00, 0x00, 0x00, 0x00};

//initialize values
float concentration_ppm = 0;
float temperature = 0;
float humidity = 0;

byte command6[] = {0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78};

// Condition values
float threshold = 10000; 

int relayPin = 8;

float temp_threshold = 27;

//LED light (alert light -> goes high when concentration above threshold)
int light = 7; //pin 7

//RESET button
int reset_button = 3; //pin 3

//RESET light
int reset_light = 4; // pin 4

//define states
enum State {
  STATE_SAFE,
  STATE_ALERT, 
  STATE_RESETTING
};

//initialize current state to regualr (safe)
State current_state = STATE_SAFE;

void setup() {
  // put your setup code here, to run once:
  
  //Init LED GPIO
  //pinMode(led, OUTPUT);
  //Init UART & baudrate
  
  //default:
  Serial.begin(9600, SERIAL_8N1);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);

  //LED light
  pinMode(light, OUTPUT);

  //RESET button
  pinMode(reset_button, INPUT);

  digitalWrite(light, HIGH);

  delay(500);
  digitalWrite(light, LOW);
}

void read_and_print_concentration() {
    for (int a = 0; a < sizeof(readIncoming_12); a++){
      readIncoming_12[a] = 0x00;
    }

    if(Serial.write(command6, sizeof(command6))){
      delay(50);
      Serial.readBytes(readIncoming_12, sizeof(readIncoming_12));
    }

    concentration_ppm = ((readIncoming_12[6]*256) + readIncoming_12[7])/1000;
    temperature = (float)((int)((readIncoming_12[8] << 8) | readIncoming_12[9]))/100;
    humidity = (float)((int)((readIncoming_12[10] << 8) | readIncoming_12[11]))/100;
    
    //can comment in or out these print statements to Serial Monitor
    Serial.print("\nConcentration:");
    Serial.print(concentration_ppm);
    Serial.print("\nTemperature:");
    Serial.print(temperature);
    Serial.print("\nHumidity:");
    Serial.print(humidity);
    Serial.println("\n");
}

void loop() {
  // put your main code here, to run repeatedly:
  read_and_print_concentration();

  switch(current_state) {
    case STATE_SAFE:
      if (concentration_ppm >= threshold) { //concentration over threshold -> bad
        current_state = STATE_ALERT; //change state

        digitalWrite(relayPin, LOW); //turn OFF relay
        digitalWrite(light, HIGH); //turn ON alert light
      } 
      break;
    
    case STATE_ALERT:
      if (digitalRead(reset_button) == HIGH) { //button is pressed
        Serial.print("\nbutton pressed");
        current_state = STATE_RESETTING;

        digitalWrite(reset_light, HIGH);  //turn ON reset light (system thinking to detect concentration)
      } 
      else {
        current_state = STATE_ALERT;  //stay in alert if no reset button attempt
      }
      break;
    
    case STATE_RESETTING: 
      if (concentration_ppm < threshold) { //concentration under threshold -> good
        current_state = STATE_SAFE; //go back to normal
        digitalWrite(relayPin, HIGH); //turn ON relay
        digitalWrite(light, LOW); //turn OFF light

        digitalWrite(reset_light, LOW); //turn OFF reset light (system done detecting concentration)
      }
      else {  //concentration above threshold -> bad
        current_state = STATE_ALERT; //if concentration is still above threshold, go to alert state

        digitalWrite(reset_light, LOW); //turn OFF reset light (system done detecting concentration)
      }
      break;
  }
  
  delay(5000);
}