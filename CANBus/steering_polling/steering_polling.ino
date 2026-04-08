#include <CAN.h>
#include <Bounce2.h>
#include "SPI.h"
#include "TFT_eSPI.h"

//CAN PINS
#define TX_GPIO_NUM   32
#define RX_GPIO_NUM   33
#define CAN_ID        0x10
#define PT_CAN_ID     0x3F

#define HAZARDS_CAN_MASK      0x00
#define BR_CAN_MASK           0x01
#define BL_CAN_MASK           0x02
#define HEADLIGHTS_CAN_MASK   0x04
#define WIPER_CAN_MASK        0x08
#define HORN_CAN_MASK         0x10

//INPUT BUTTONS
#define NUM_BUTTONS       6

#define HAZARDS_PIN       26
#define BLINK_R_PIN       27
#define BLINK_L_PIN       12
#define HEADLIGHTS_PIN    25
#define WIPER_PIN         22
#define HORN_PIN          13

const uint8_t button_pins[NUM_BUTTONS] = {HAZARDS_PIN, BLINK_R_PIN, BLINK_L_PIN, HEADLIGHTS_PIN, WIPER_PIN, HORN_PIN};
const uint32_t CAN_masks[NUM_BUTTONS] = {HAZARDS_CAN_MASK, BR_CAN_MASK, BL_CAN_MASK, HEADLIGHTS_CAN_MASK, WIPER_CAN_MASK, HORN_CAN_MASK};
Bounce * buttons_debounced = new Bounce[NUM_BUTTONS];
TFT_eSPI tft = TFT_eSPI();

char devices_enable = 0x00;
uint8_t PT_data_buf = 0x00;
uint32_t RPM = 0x00;
bool button_changed = false;
bool hazards_on = false;
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

  // start the CAN bus at 500 kbps -- halved? 250kbps
  if (!CAN.begin(500E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }
  tft.init();
  tft.fillScreen(TFT_BLACK);

  tft.setRotation(3);
  utsm();
  speed();
  drawIcons(devices_enable);
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
      button_changed = true;

      if(i == 0){
        //if hazards, toggle BR and BL
        if(!hazards_on){
          //hazards OFF, so turn ON hazards and set blinkers ON
          devices_enable |= CAN_masks[1];
          devices_enable |= CAN_masks[2];
        } else{
          //hazards ON, so turn OFF hazards and set blinkers OFF only if Blinkers are not ON.
          if(digitalRead(BLINK_R_PIN) == HIGH) devices_enable &= ~(CAN_masks[1]);
          if(digitalRead(BLINK_L_PIN) == HIGH) devices_enable &= ~(CAN_masks[2]);
        }
        hazards_on = !hazards_on;
      } 
      else if (i == 3) {
        //if headlights, toggle
        devices_enable ^= CAN_masks[3];
      }
      else {
        devices_enable |= CAN_masks[i];
      }
      if(i == 1 || i == 2){
        if(hazards_on) button_changed = false;
      }
    } else if (buttons_debounced[i].rose()){
      //button released, clear bits to 0
      if(i == 0 || i == 3){
        //if hazards, check BR and HL
        //if(digitalRead(BLINK_L_PIN) == HIGH) devices_enable &= ~(CAN_masks[2]);
        //if(digitalRead(BLINK_R_PIN) == HIGH) devices_enable &= ~(CAN_masks[1]);
        button_changed = false;
      }
      else if (i == 1 || i == 2){
        //only clear blinkers if HAZARDS are OFF
        if(!hazards_on) {
          devices_enable &= ~(CAN_masks[i]);
          button_changed = true;
        }
      } 
      else {
        devices_enable &= ~(CAN_masks[i]);
        button_changed = true;
      }
    }
  }

  if(button_changed){
    Serial.printf("Sending packet 0x%02X ... ", (unsigned char) devices_enable);

    CAN.beginPacket(CAN_ID);
    CAN.write(devices_enable);
    CAN.endPacket();

    drawIcons(devices_enable);

    Serial.println("done");
    button_changed = false;
  }

    // try to parse CAN packet
  int packetSize = CAN.parsePacket();
  if (packetSize) {
    RPM = 0;
    int bytes_read = 0;
    // received a packet
    Serial.print ("Received ");

    if (CAN.packetExtended()) {
      Serial.print ("extended ");
    }

    if (CAN.packetRtr()) {
      // Remote transmission request, packet contains no data
      Serial.print ("RTR ");
    }
    uint8_t received_ID = CAN.packetId();
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
          // current_device_states = (uint8_t) CAN.read();
          // Serial.printf("0x%02X", current_device_states);
        } else {
          PT_data_buf = (uint8_t) CAN.read();
          bytes_read++;
          Serial.printf("0x%02X  ", PT_data_buf);
          if(bytes_read <= 4){
            RPM += (PT_data_buf << (8 * (4-bytes_read)));
            Serial.printf("0x%08X\n", RPM);
            Serial.printf("%d", bytes_read);
          }
        }
      Serial.println();
      }
      speed_num((int) RPM);
    }
    Serial.println();
  }
}

unsigned long utsm() {
  tft.fillScreen(TFT_BLACK);
  unsigned long start = micros();
  tft.setCursor(140, 100);
  tft.setTextColor(TFT_GREEN);  tft.setTextSize(4);
  tft.println("Hello UTSM!");
  return micros() - start;
}

unsigned long speed() {
  unsigned long start = micros();
  tft.setCursor(140, 160);
  tft.setTextColor(TFT_WHITE);  tft.setTextSize(3);
  tft.println("Speed: ");
  return micros() - start;
}

unsigned long speed_num(int sp) {
  tft.fillRect(250, 160, 50, 20, TFT_RED);
  unsigned long start = micros();
  char buf[10];
  sprintf(buf, "%d", sp);
  tft.setCursor(255, 165);
  tft.setTextColor(TFT_WHITE);  tft.setTextSize(2);
  tft.println(buf);
  return micros() - start;
}

void drawIcons(uint8_t state) {
  static uint8_t lastState = 0xFF;
  if (state == lastState) return;
  lastState = state;

  int w = tft.width();
  int h = tft.height();

  int box = 40;

  // ===== CLEAR ALL ICON AREAS =====
  // Left side
  tft.fillRect(0, 0, box, box, TFT_BLACK);                 // top-left
  tft.fillRect(0, h/2 - box/2, box, box, TFT_BLACK);       // mid-left
  tft.fillRect(0, h - box, box, box, TFT_BLACK);           // bottom-left

  // Right side
  tft.fillRect(w - box, 0, box, box, TFT_BLACK);           // top-right
  tft.fillRect(w - box, h/2 - box/2, box, box, TFT_BLACK); // mid-right
  tft.fillRect(w - box, h - box, box, box, TFT_BLACK);     // bottom-right

  // ===== LEFT SIDE =====

  // Headlights (top-left)
  if (!digitalRead(HEADLIGHTS_PIN)) {
    tft.fillCircle(15, 15, 8, TFT_YELLOW);
    tft.drawLine(25, 10, 35, 5, TFT_YELLOW);
    tft.drawLine(25, 15, 35, 15, TFT_YELLOW);
    tft.drawLine(25, 20, 35, 25, TFT_YELLOW);
  }

  // Wiper (bottom-left)
  if (!digitalRead(WIPER_PIN)) {
    int y = h - 20;
    tft.drawLine(5, y, 35, y - 10, TFT_WHITE);
    tft.drawLine(5, y + 5, 35, y - 5, TFT_WHITE);
  }

  // ===== RIGHT SIDE =====

  // Turn Right (top-right)
  if (!digitalRead(BLINK_R_PIN)) {
    tft.fillTriangle(w - 10, 20, w - 30, 10, w - 30, 30, TFT_GREEN);
  }

 // LEFT TURN (pressed = LOW)
  if (!digitalRead(BLINK_L_PIN)) {
    tft.fillTriangle(w - 50, 20, w - 30, 10, w - 30, 30, TFT_GREEN);
  }

  // Hazard (mid-right)
  if (!digitalRead(HAZARDS_PIN)) {
    int cx = w - box/2;
    int cy = h/2;
    tft.fillTriangle(cx, cy - 10, cx - 10, cy + 10, cx + 10, cy + 10, TFT_ORANGE);
  }

  // Horn (bottom-right)
  if (!digitalRead(HORN_PIN)) {
    // Base position for bottom-right corner
    int cx = w - 30; // center X
    int cy = h - 15; // center Y

    // Right-pointing triangle (replaces circle)
    // Same "radius" as headlight circle (8px)
    tft.fillTriangle(cx - 8, cy - 8, cx - 8, cy + 8, cx + 8, cy, TFT_RED);

    // Three lines (same offsets as headlight lines)
    tft.drawLine(cx + 10, cy - 6, cx + 20, cy - 10, TFT_RED);
    tft.drawLine(cx + 10, cy,     cx + 22, cy,      TFT_RED);
    tft.drawLine(cx + 10, cy + 6, cx + 20, cy + 10, TFT_RED);
  }
}
