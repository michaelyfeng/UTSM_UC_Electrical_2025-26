#include <CAN.h>
#include <Bounce2.h>

#define TX_GPIO_NUM   32
#define RX_GPIO_NUM   33
#define CAN_ID        0x09

#define BLINK_R       21
#define BLINK_L       14
#define HEADLIGHT_R   25
#define HEADLIGHT_L   15
#define WIPER         13 //16 for servo
#define HORN          27
#define BRAKE_SW      12

#define HAZARDS_CAN_MASK      0x00
#define BR_CAN_MASK           0x01
#define BL_CAN_MASK           0x02
#define HEADLIGHTS_CAN_MASK   0x04
#define WIPER_CAN_MASK        0x08
#define HORN_CAN_MASK         0x10
#define BRAKE_SW_CAN_MASK     0x20

#define BR_BIT_POS            0
#define BL_BIT_POS            1
#define HEADLIGHTS_BIT_POS    2
#define WIPER_BIT_POS         3
#define HORN_BIT_POS          4
#define BRAKE_SW_BIT_POS      5

#define BLINK_FREQ    2
#define ADB_CLK_FREQ  80000000 //80MHz
#define PRESCALER 8

Bounce brake_switch_pin = Bounce();

uint8_t last_device_states = 0x00;
uint8_t current_device_states = 0x00;
bool BR_flag = false;
bool BL_flag = false;
bool blink_toggle_state = true;
hw_timer_t *blinkTimer = NULL;

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
  pinMode(HEADLIGHT_R, OUTPUT);
  pinMode(HEADLIGHT_L, OUTPUT);
  pinMode(WIPER, OUTPUT);
  pinMode(HORN, OUTPUT);

  //define INPUT devices
  brake_switch_pin.attach(BRAKE_SW, INPUT_PULLUP);
  brake_switch_pin.interval(50);
}

void loop() {
  //poll brake switch
  brake_switch_pin.update();
  if(brake_switch_pin.fell()){
    //Pressed
    current_device_states |= (uint8_t)BRAKE_SW_CAN_MASK;

    Serial.printf("Sending packet 0x%02X ... ", (unsigned char) current_device_states);

    CAN.beginPacket(CAN_ID);
    CAN.write(current_device_states);
    CAN.endPacket();

    Serial.println("done");

  } else if(brake_switch_pin.rose()) {
    //released
    current_device_states &= ~((uint8_t)BRAKE_SW_CAN_MASK);

    Serial.printf("Sending packet 0x%02X ... ", (unsigned char) current_device_states);

    CAN.beginPacket(CAN_ID);
    CAN.write(current_device_states);
    CAN.endPacket();

    Serial.println("done");
  }

  // try to parse CAN packet
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

    Serial.print ("packet with id 0x");
    Serial.print (CAN.packetId(), HEX);

    if (CAN.packetRtr()) {
      Serial.print (" and requested length ");
      Serial.println (CAN.packetDlc());
    } else {
      Serial.print (" and length ");
      Serial.println (packetSize);

      // only print packet data for non-RTR packets
      while (CAN.available()) {
        current_device_states = (uint8_t) CAN.read();
        Serial.printf("0x%02X", current_device_states);
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

    if(0x1 & (current_device_states >> HEADLIGHTS_BIT_POS)){
      digitalWrite(HEADLIGHT_L, HIGH);
      digitalWrite(HEADLIGHT_R, HIGH);
    } else {
      digitalWrite(HEADLIGHT_L, LOW);
      digitalWrite(HEADLIGHT_R, LOW);
    }

    if(0x1 & (current_device_states >> HORN_BIT_POS)){
      digitalWrite(HORN, HIGH);
    } else digitalWrite(HORN, LOW);

    if(0x1 & (current_device_states >> WIPER_BIT_POS)){
      //servo stuff
      digitalWrite(WIPER, HIGH);
    } else digitalWrite(WIPER, LOW);
  }
}