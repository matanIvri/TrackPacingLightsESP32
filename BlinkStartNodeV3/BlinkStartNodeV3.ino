#include <LoRaWan_APP.h>
#include <Arduino.h>

#define NODE_ID               3          // unique per node

#define RF_FREQUENCY          915000000  // Hz
#define LORA_BANDWIDTH        0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE       1
#define LORA_PREAMBLE_LENGTH  8
#define LORA_FIX_LENGTH       false
#define LORA_IQ_INVERSION     false
#define LORA_SYMBOL_TIMEOUT   0

#define RX_TIMEOUT_VALUE      10000      // single RX timeout

// default blink settings
const uint32_t ON_DUR  = 500;   // ms LED on each blink
const uint32_t OFF_DUR = 500;   // ms LED off
const uint8_t  REPS    = 3;     // blinks per wave pulse
const uint32_t BLINK_DUR = (ON_DUR + OFF_DUR) * REPS;

static RadioEvents_t RadioEvents;
volatile bool       gotPacket = false;

// parameters from gateway
uint8_t  TOTAL_LAPS = 0;
uint32_t WAVE_PERIOD = 0;
uint32_t OFFSET_MS   = 0;
uint8_t START_ND = 0;
uint8_t NODES_COUNT = 4;

// calculated
uint8_t NEW_ID;

// forward declaration
void processWavePacket();

// LoRa RX callback: unpack payload
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  if (size == 11) {
    TOTAL_LAPS = payload[0];
    WAVE_PERIOD = ((uint32_t)payload[1] << 24)
                | ((uint32_t)payload[2] << 16)
                | ((uint32_t)payload[3] <<  8)
                |  (uint32_t)payload[4];
    OFFSET_MS = ((uint32_t)payload[5] << 24)
              | ((uint32_t)payload[6] << 16)
              | ((uint32_t)payload[7] <<  8)
              |  (uint32_t)payload[8];
    START_ND = payload[9];
    NODES_COUNT = payload[10];

    NEW_ID = (NODE_ID + NODES_COUNT - START_ND) % NODES_COUNT;

    Serial.printf("Node[%u]: RX %u bytes – laps=%u period=%lums offset=%lums (RSSI %d, SNR %d)\n",
                  NODE_ID, size, TOTAL_LAPS, WAVE_PERIOD, OFFSET_MS, rssi, snr);
    Serial.printf("start node: %u, nodes count: %u, newID: %u\n", START_ND, NODES_COUNT, NEW_ID);

    gotPacket = true;
  } else {
    Serial.printf("Node[%u]: unexpected size %u\n", NODE_ID, size);
  }
}

void OnRxTimeout() {
  // no action—continuous RX is re‑armed automatically
}

// Helper: non‑blocking wait that still processes LoRa IRQs and can break on new packet
void smartDelay(uint32_t ms) {
  uint32_t start = millis();
  while ((millis() - start) < ms) {
    Radio.IrqProcess();          // let LoRa run
    if (gotPacket) return;      // interrupt if new data arrives
  }
}

// Blink one wave pulse (REPS on/off), interruptible
void runBlink() {
  for (uint8_t i = 0; i < REPS; i++) {
    if (gotPacket) return;      // stop immediately if new packet
    digitalWrite(LED_BUILTIN, HIGH);
    smartDelay(ON_DUR);
    digitalWrite(LED_BUILTIN, LOW);
    if (gotPacket) return;
    smartDelay(OFF_DUR);  // stop immediately if new packet
  }
}

// Called whenever gotPacket is true: stagger, then run laps interruptibly
void processWavePacket() {
  uint32_t timeToStart = 0;
  int32_t waveOff;

  // clear flag now to allow new interruptions
  gotPacket = false;

  //check if the recieved data is a top sign
  if(TOTAL_LAPS == 0 || WAVE_PERIOD == 0 || OFFSET_MS == 0){
    //stop sign
    Serial.println("Stop");
    digitalWrite(LED_BUILTIN, LOW);
  }

  // stagger start by node-specific offset
  // The last blink (out of 3) should start in the pacing time, so the blink should start before
  if(NEW_ID == 0){
    timeToStart =  WAVE_PERIOD - (ON_DUR * 2) - BLINK_DUR + ON_DUR;

    //blink once for 1 sec, wait, and start the wave.
    digitalWrite(LED_BUILTIN, HIGH);
    smartDelay(ON_DUR * 2);
    digitalWrite(LED_BUILTIN, LOW);
  }
  else {
    timeToStart = NEW_ID * OFFSET_MS - (BLINK_DUR - ON_DUR);
  }
  Serial.printf("Node[%u]: delaying %lums before wave start\n", NODE_ID, timeToStart);
  smartDelay(timeToStart);
  if (gotPacket) return;

  // compute off‑duration between pulses
  waveOff = WAVE_PERIOD - BLINK_DUR;

  // run TOTAL_LAPS pulses
  for (uint8_t lap = 0; lap < TOTAL_LAPS; lap++) {
    runBlink();               // may exit early on new packet
    if (gotPacket) return;    // break immediately if new data
    smartDelay(waveOff);
    if (gotPacket) return;
  }

  Serial.printf("Node[%u]: wave complete, back to RX\n", NODE_ID);
}

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  pinMode(LED_BUILTIN, OUTPUT);

  // register LoRa callbacks
  RadioEvents.RxDone    = OnRxDone;
  RadioEvents.RxTimeout = OnRxTimeout;

  // init LoRa in continuous RX
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(
    MODEM_LORA,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR,
    LORA_CODINGRATE,
    0,                   // unused
    LORA_PREAMBLE_LENGTH,
    LORA_SYMBOL_TIMEOUT,
    LORA_FIX_LENGTH,
    9,                   // payload length
    true,                // CRC on
    false,               // no freq hop
    0,
    LORA_IQ_INVERSION,
    true                 // continuous RX
  );

  Serial.printf("Node[%u]: Listening for gateway broadcasts…\n", NODE_ID);
  Radio.Rx(RX_TIMEOUT_VALUE);
}

void loop() {
  // process incoming events
  Radio.IrqProcess();

  // if a packet arrived, handle it (stagger + wave), interruptible
  if (gotPacket) {
    processWavePacket();
  }

  delay(10);
}
