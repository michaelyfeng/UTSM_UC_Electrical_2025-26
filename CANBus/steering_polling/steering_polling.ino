#include <CAN.h>
#include <Bounce2.h>

//CAN PINS
#define TX_GPIO_NUM   32
#define RX_GPIO_NUM   33
#define CAN_ID        0x10

#define HAZARDS_CAN_MASK      0x00
#define BR_CAN_MASK           0x01
#define BL_CAN_MASK           0x02
#define HEADLIGHTS_CAN_MASK   0x04
#define WIPER_CAN_MASK        0x08
#define HORN_CAN_MASK         0x10

//INPUT BUTTONS
#define NUM_BUTTONS       6

#define HAZARDS_PIN       26
#define BLINK_R_PIN       12
#define BLINK_L_PIN       27
#define HEADLIGHTS_PIN    25
#define WIPER_PIN         22
#define HORN_PIN          21

const uint8_t button_pins[NUM_BUTTONS] = {HAZARDS_PIN, BLINK_R_PIN, BLINK_L_PIN, HEADLIGHTS_PIN, WIPER_PIN, HORN_PIN};
const uint32_t CAN_masks[NUM_BUTTONS] = {HAZARDS_CAN_MASK, BR_CAN_MASK, BL_CAN_MASK, HEADLIGHTS_CAN_MASK, WIPER_CAN_MASK, HORN_CAN_MASK};
Bounce * buttons_debounced = new Bounce[NUM_BUTTONS];

char devices_enable = 0x00;
bool button_changed = false;
uint32_t gpioStates = 0x0;
bool state = true;
int num_changes = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  for(int i = 0; i < NUM_BUTTONS; i++){
    buttons_debounced[i].attach(button_pins[i], INPUT_PULLUP);
    buttons_debounced[i].interval(50);
  }

  Serial.begin (115200);
  while (!Serial);
  delay (100);

  Serial.println ("CAN Sender");

  // Set the pins
  CAN.setPins (RX_GPIO_NUM, TX_GPIO_NUM);

  // start the CAN bus at 500 kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }

  // for(int i = 0; i < NUM_BUTTONS; i++){
  //   Serial.println()
  // }
}

void loop() {

  //poll buttons
  for(int i = 0; i < NUM_BUTTONS; i++){
    buttons_debounced[i].update();
    if(buttons_debounced[i].fell()){
      //button pressed, set bit to 1
      if(i == 0){
        //if hazards, set BR and BL
        devices_enable |= CAN_masks[2];
        devices_enable |= CAN_masks[1];
      } else {
        devices_enable |= CAN_masks[i];
      }
      button_changed = true;
    } else if (buttons_debounced[i].rose()){
      //button released, clear bits to 0
      if(i == 0){
        //if hazards, check BR and HL
        if(digitalRead(BLINK_L_PIN) == HIGH) devices_enable &= ~(CAN_masks[2]);
        if(digitalRead(BLINK_R_PIN) == HIGH) devices_enable &= ~(CAN_masks[1]);
      } else if (i == 1 || i == 2){
          if(digitalRead(HAZARDS_PIN) == HIGH) devices_enable &= ~(CAN_masks[i]);
      } else {
        devices_enable &= ~(CAN_masks[i]);
      }
      button_changed = true;
    }
  }

  if(button_changed){
    Serial.printf("Sending packet 0x%02X ... ", (unsigned char) devices_enable);

    CAN.beginPacket(CAN_ID);
    CAN.write(devices_enable);
    CAN.endPacket();

    Serial.println("done");
    button_changed = false;
  }
}
