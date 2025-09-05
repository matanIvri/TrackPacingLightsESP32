//In this version the Blink ends in when the runners are on pace,
// so the last blink turns on 0.5 sec before the start

#include <Arduino.h>
#include <LoRaWan_APP.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

//gateway ID is like node 0
#define ID 0

// —————————————————
// UUIDs for our custom BLE service
// —————————————————
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// —————————————————
// LoRa radio settings
// —————————————————
#define RF_FREQUENCY            915000000  // Hz
#define TX_OUTPUT_POWER         20         // dBm
#define LORA_BANDWIDTH          0
#define LORA_SPREADING_FACTOR   7
#define LORA_CODINGRATE         1
#define LORA_PREAMBLE_LENGTH    8
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON    false

// —————————————————
// Blink parameters (fixed)
// —————————————————
const uint32_t ON_DUR  = 500;   // ms LED on
const uint32_t OFF_DURATION = 500;   // ms LED off
const uint8_t  REPS         = 3;     // number of blinks per wave
const uint32_t BLINK_DUR =  (ON_DUR + OFF_DURATION) * REPS; //total blinking sequence time
const uint32_t ON_DUR_LONG  = 1000;   // ms LED on for long cases (first wave)

// —————————————————
// Variable parameters (updated via BLE)
// —————————————————
uint8_t  TOTAL_LAPS = -1;      // number of waves to run
uint32_t WAVE_PERIOD = -1;  // ms between the start of each wave
uint8_t  NODES_COUNT = 1;      // how many nodes in the system
uint8_t START_ND = 0;
uint32_t OFFSET = 0; //in ms
uint8_t NEW_ID = 0;

// —————————————————
// Flag to trigger new wave in loop()
// —————————————————
volatile bool newParams = false;

// —————————————————
// LoRa radio state & callbacks
// —————————————————
bool     lora_idle = true;
static RadioEvents_t RadioEvents;

void OnTxDone(void) {
  Serial.println("LoRa TX done");
  lora_idle = true;
}

void OnTxTimeout(void) {
  Serial.println("LoRa TX timeout");
  lora_idle = true;
}

// —————————————————
// BLE server connect/disconnect callbacks
// —————————————————
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {}
  void onDisconnect(BLEServer* pServer) override {
    Serial.println("Client disconnected, restarting advertising");
    pServer->getAdvertising()->start();
  }
};

// —————————————————
// BLE write handler: parse "TOTAL_LAPS,WAVE_PERIOD,NODES_COUNT"
// —————————————————
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    if (value.length() == 0) return;

    // Convert String to C string for sscanf
    const char* payload = value.c_str();

    // Temporaries for parsing
    int lapsInt, nodesInt, startNode;
    float periodSec;
    if (sscanf(payload, "%d,%f,%d,%d", &lapsInt, &periodSec, &nodesInt, &startNode) != 4) {
      Serial.println("BLE format error: use TOTAL_LAPS,WAVE_PERIOD(s),NODES_COUNT");
      return;
    }

    // Commit to globals
    TOTAL_LAPS  = (uint8_t)lapsInt;
    WAVE_PERIOD = (uint32_t)(periodSec * 1000.0f);
    NODES_COUNT = (uint8_t)nodesInt;
    START_ND = (uint8_t)startNode;
    if(NODES_COUNT != 0){
    OFFSET = WAVE_PERIOD / NODES_COUNT;

    //newID = (ID + number of nodes - start node) % number of nodes
    NEW_ID = (ID + NODES_COUNT - START_ND) % NODES_COUNT;
    }

    // Feedback
    Serial.println("=== New BLE params ===");
    Serial.printf("Laps=%u, Period=%.2f s (%u ms), Nodes=%u, Offset=%u ms, newID=%u\n",
                  TOTAL_LAPS, periodSec, WAVE_PERIOD, NODES_COUNT, OFFSET, NEW_ID);
    Serial.println("======================");

    // Signal loop() to run a new wave (and interrupt any current one)
    newParams = true;
  }
};

// —————————————————
// Function prototypes
// —————————————————
void sendLoraAndWave();
void runBlink();
void smartDelay(uint32_t ms);

// —————————————————
// Arduino setup()
// —————————————————
void setup() {
  // Initialize Serial for debug
  Serial.begin(115200);
  delay(100);

  // Initialize LoRa radio
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  RadioEvents.TxDone    = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(
    MODEM_LORA, TX_OUTPUT_POWER, 0,
    LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODINGRATE,
    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
    true, 0, 0, LORA_IQ_INVERSION_ON, 3000
  );
  pinMode(LED_BUILTIN, OUTPUT);

  // Initialize BLE server
  BLEDevice::init("PacingLightsHead");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic* pChar = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ  |
    BLECharacteristic::PROPERTY_WRITE
  );
  pChar->setCallbacks(new MyCallbacks());
  pChar->setValue("ready");
  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("BLE gateway ready. Write TOTAL_LAPS,WAVE_PERIOD(s),NODES_COUNT");
}

// —————————————————
// Arduino loop(): handle BLE reconnect & new waves
// —————————————————
void loop() {
  // Process LoRa interrupts (TX done/timeout/RX not used here)
  Radio.IrqProcess();

  // If onWrite signaled newParams, interrupt current wave and start new
  if (newParams) {
    newParams = false;
    if(TOTAL_LAPS == 0 || WAVE_PERIOD == 0 || NODES_COUNT == 0){
      //stop wave
      stopWave();
    }
    sendLoraAndWave();
  }

  delay(10);
}

// —————————————————
// Stop the wave, turn off the lights, send buffer 0 to signal other nodes to stop
// —————————————————
void stopWave(){
  Serial.print("Stop wave");
  digitalWrite(LED_BUILTIN, LOW); //turn off LED

  //send buf full of 0's
  uint8_t buf[9] = {0};
  lora_idle = false;
  Radio.Send(buf, sizeof(buf));
  while (!lora_idle) {
    Radio.IrqProcess();
  }
}

// —————————————————
// Pack & send over LoRa, then run the LED wave interruptibly
// —————————————————
void sendLoraAndWave() {
  uint32_t waveOff;
  uint32_t timeToStart = 0;

  // 1) Send parameters via LoRa
  uint8_t buf[11];
  buf[0] = TOTAL_LAPS;
  buf[1] = WAVE_PERIOD >> 24;
  buf[2] = WAVE_PERIOD >> 16;
  buf[3] = WAVE_PERIOD >>  8;
  buf[4] = WAVE_PERIOD;
  buf[5] = OFFSET >> 24;
  buf[6] = OFFSET >> 16;
  buf[7] = OFFSET >>  8;
  buf[8] = OFFSET;
  buf[9] = START_ND;
  buf[10] = NODES_COUNT;

  Serial.print("LoRa payload: ");
  for (int i = 0; i < 9; i++) Serial.printf("%02X ", buf[i]);
  Serial.println();

  lora_idle = false;
  Radio.Send(buf, sizeof(buf));
  while (!lora_idle) {
    Radio.IrqProcess();
  }

  // 2) Run the wave pulses, but break out on newParams
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
    timeToStart = NEW_ID * OFFSET - (BLINK_DUR - ON_DUR);
  }
  Serial.printf("Gateway: delaying %lums before wave start\n", timeToStart);
  smartDelay(timeToStart);
  if (newParams) return;

  // compute off‑duration between pulses
  waveOff = WAVE_PERIOD - BLINK_DUR;

  // run TOTAL_LAPS pulses
  for (uint8_t lap = 0; lap < TOTAL_LAPS; lap++) {
    runBlink();               // may exit early on new packet
    if (newParams) return;    // break immediately if new data
    smartDelay(waveOff);
    if (newParams) return;
  }
  /*
  uint32_t waveOff = WAVE_PERIOD - BLINK_DUR;

  // The last blink (out of 3) should start in the pacing time, so the blink should start before  
  uint32_t timeToStart =  WAVE_PERIOD - ON_DUR_LONG - BLINK_DUR + ON_DUR;

  //blink once for 1 sec, wait, and start the wave.
  digitalWrite(LED_BUILTIN, HIGH);
  smartDelay ON_DUR_LONG);
  digitalWrite(LED_BUILTIN, LOW);

  smartDelay(timeToStart);

  for (uint8_t lap = 0; lap < TOTAL_LAPS; lap++) {
    if (newParams) return;    // interrupt if new data arrives
    runBlink();               // uses smartDelay internally
    if (newParams) return;
    smartDelay(waveOff);
  }
  */
}

// —————————————————
// Blink helper: REPS on/off cycles, interruptible
// —————————————————
void runBlink() {
  for (uint8_t i = 0; i < REPS; i++) {
    if (newParams) return;
    digitalWrite(LED_BUILTIN, HIGH);
    smartDelay (ON_DUR);
    digitalWrite(LED_BUILTIN, LOW);
    if (newParams) return; //new params were sent - so stop blinking
    smartDelay(OFF_DURATION);
  }
}

// —————————————————
// Delay that still services LoRa IRQs and checks for newParams
// —————————————————
void smartDelay(uint32_t ms) {
  uint32_t start = millis();
  while ((millis() - start) < ms) {
    Radio.IrqProcess();
    if (newParams) return;
  }
}
