#include <CAN.h>
#include <Adafruit_NeoPixel.h>

#define TX_GPIO_NUM   32
#define RX_GPIO_NUM   33
#define CAN_ID        0x08
#define PT_CAN_ID     0x99

#define BLINK_R       21
#define BLINK_L       14
#define BRAKE         26

#define NUM_BRAKE_LEDS 78

#define BR_CAN_MASK           0x01
#define BL_CAN_MASK           0x02
#define BRAKE_CAN_MASK        0x20

#define BR_BIT_POS            0
#define BL_BIT_POS            1
#define BRAKE_BIT_POS         5

#define BLINK_FREQ    2
#define ADB_CLK_FREQ  80000000 //80MHz
#define PRESCALER 8

uint8_t last_device_states = 0x00;
uint8_t current_device_states = 0x00;
uint8_t PT_data = 0x00;
bool BR_flag = false;
bool BL_flag = false;
bool blink_toggle_state = true;
hw_timer_t *blinkTimer = NULL;
Adafruit_NeoPixel brake_leds(NUM_BRAKE_LEDS, BRAKE, NEO_GRB + NEO_KHZ800);

void ARDUINO_ISR_ATTR toggleBlink(){
  //toggle blink if light ON
  //Serial.println("in toggleBlink!");
  if(BR_flag){
    digitalWrite(BLINK_R, blink_toggle_state);
  }
  if(BL_flag){
    digitalWrite(BLINK_L, blink_toggle_state);
  }
  blink_toggle_state = !blink_toggle_state;
}

void setup() {
  Serial.begin (115200);
  while (!Serial);
  delay (100);

  Serial.println ("CAN Receiver");

  // Set the pins
  CAN.setPins (RX_GPIO_NUM, TX_GPIO_NUM);

  // start the CAN bus at 500 kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }

  //setup timer
  blinkTimer = timerBegin(0, PRESCALER, true);
  if(blinkTimer == NULL) {
    Serial.println("Timer congfig failed");
    while(1);
  }
  timerStop(blinkTimer);
  timerAttachInterrupt(blinkTimer, &toggleBlink, true);
  timerAlarmWrite(blinkTimer, (int) ADB_CLK_FREQ/PRESCALER/BLINK_FREQ, true);
  timerAlarmEnable(blinkTimer);
  Serial.printf("Configured timer at %d Hz", (int)BLINK_FREQ);

  //define OUTPUT devices
  pinMode(BLINK_R, OUTPUT);
  pinMode(BLINK_L, OUTPUT);
  pinMode(BRAKE, OUTPUT);

  brake_leds.begin();
  brake_leds.show();
  for(int i = 0; i < NUM_BRAKE_LEDS; i++){
    brake_leds.setPixelColor(i, brake_leds.Color(255, 0, 0));
    //brake_leds.show();
  }
  brake_leds.setBrightness(30);
  brake_leds.show();
}

void loop() {
  // try to parse packet
  int packetSize = CAN.parsePacket();

  if (packetSize) {
    // received a packet
    Serial.print ("Received ");

    if (CAN.packetExtended()) {
      Serial.print ("extended ");
    }

    if (CAN.packetRtr()) {
      // Remote transmission request, packet contains no data
      Serial.print ("RTR ");
    }

    uint32_t received_ID= CAN.packetId();
    Serial.print ("packet with id 0x");
    Serial.print (received_ID, HEX);
    
    if (CAN.packetRtr()) {
      Serial.print (" and requested length ");
      Serial.println (CAN.packetDlc());
    } else {
      Serial.print (" and length ");
      Serial.println (packetSize);

      // only print packet data for non-RTR packets
      while (CAN.available()) {
        if(received_ID != PT_CAN_ID){
          current_device_states = (uint8_t) CAN.read();
          Serial.printf("0x%02X", current_device_states);
        } else {
          PT_data = (uint8_t) CAN.read();
          Serial.printf("0x%02X", PT_data);
          Serial.println();
        }

      }
      Serial.println();
    }

    Serial.println();
  }

  //detect change in device states
  if(last_device_states != current_device_states){

    last_device_states = current_device_states;
    if(0x1 & (current_device_states >> BR_BIT_POS)){
      BR_flag = true;
      timerStart(blinkTimer);
    } else{
      BR_flag = false;
      digitalWrite(BLINK_R, LOW);
    }

    if(0x1 & (current_device_states >> BL_BIT_POS)){
      BL_flag = true;
      timerStart(blinkTimer);
    } else{
      BL_flag = false;
      digitalWrite(BLINK_L, LOW);
    }

    if(!BR_flag && !BL_flag) timerStop(blinkTimer);

    if(0x1 & (current_device_states >> BRAKE_BIT_POS)){
      //digitalWrite(BRAKE, HIGH);
      brake_leds.setBrightness(255);
      brake_leds.show();
    } else {
      //digitalWrite(BRAKE, LOW);
      brake_leds.setBrightness(20);
      brake_leds.show();
    }
  }
}