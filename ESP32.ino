#include <EEPROM.h>
#include <DHT.h>
#include "WiFi.h"
#include "WiFiProv.h"
#include <RMaker.h> 
#include <IRremote.hpp>  


// BLE Credentils
const char *service_name = "PROV_Automation";
const char *pop = "12345678";

// Pin Definitions
static uint8_t DHTPIN = 5;
static uint8_t IR_RECEIVE_PIN = 4;
static uint8_t RESETPIN = 0;
// Relay pins
static uint8_t relayPin1 = 23;
static uint8_t relayPin2 = 22;
static uint8_t relayPin3 = 21;
static uint8_t relayPin4 = 19;
// Switch pins
static uint8_t SwitchPin1 = 27;
static uint8_t SwitchPin2 = 14;
static uint8_t SwitchPin3 = 12;
static uint8_t SwitchPin4 = 13;

int BUZZER_PIN = 18;

// Relay state tracking
bool relay1_state = false;
bool relay2_state = false;
bool relay3_state = false;
bool relay4_state = false;

bool previousSwitch1States = false;
bool previousSwitch2States = false;
bool previousSwitch3States = false;
bool previousSwitch4States = false;
// DHT Sensor setup
DHT dht(DHTPIN, DHT11);

bool wifi_connected = 0;
bool isInIRSetMode  = false;
int irSetRelay = -1;

// RainMaker device and parameters
char RoomName[] = "Study Room";
char deviceName_1[] = "Light";
char deviceName_2[] = "Desk Light";
char deviceName_3[] = "Plug";
char deviceName_4[] = "Outlet";

// Declaring Devices 
static TemperatureSensor temperature("Temperature");
static TemperatureSensor humidity("Humidity");
static LightBulb my_switch1(deviceName_1, &relayPin1);
static LightBulb my_switch2(deviceName_2, &relayPin2);
static Switch my_switch3(deviceName_3, &relayPin3);
static Switch my_switch4(deviceName_4, &relayPin4);
static Device setir("Set Remote", "custom.device.setir");
static Device unsetir("Unset Remote", "custom.device.unsetir");

void playbeep(int count = 1, int Frequency = 1500, int length = 300) {
  for (int i = 0; i < count; i++) {
    ledcSetup(0, Frequency, 8);         // Channel 0, frequency in Hz, resolution 8-bit
    ledcAttachPin(BUZZER_PIN, 0); // Attach pin to channel
    ledcWrite(0, 128);            // 50% duty cycle
    delay(length);              // Play tone for the specified duration
    ledcWrite(0, 0);              // Stop the tone
    delay(100);
  }
  noTone(BUZZER_PIN);
}

void sysProvEvent(arduino_event_t *sys_event)
{
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_PROV_START:
  #if CONFIG_IDF_TARGET_ESP32
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
        printQR(service_name, pop, "ble");
  #else
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
        printQR(service_name, pop, "softap");
  #endif
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      playbeep(1, 1500, 400);
      // Serial.printf("\nConnected to Wi-Fi!\n");
      wifi_connected = 1;
      delay(500);
      break;
    case ARDUINO_EVENT_PROV_CRED_RECV: {
        Serial.println("\nReceived Wi-Fi credentials");
        Serial.print("\tSSID : ");
        Serial.println((const char *) sys_event->event_info.prov_cred_recv.ssid);
        Serial.print("\tPassword : ");
        Serial.println((char const *) sys_event->event_info.prov_cred_recv.password);
        break;
      }
    case ARDUINO_EVENT_PROV_INIT:
      wifi_prov_mgr_disable_auto_stop(10000);
      break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      Serial.println("Stopping Provisioning!!!");
      wifi_prov_mgr_stop_provisioning();
      break;
  }
}

// Debouncing Timers
unsigned long lastToggleTimes1 = 0;
unsigned long lastToggleTimes2 = 0;
unsigned long lastToggleTimes3 = 0;
unsigned long lastToggleTimes4 = 0;
const unsigned long debounceDelay = 500;

// Function to save relay states to EEPROM
void saveRelayStates() {
  EEPROM.write(1, relay1_state);
  EEPROM.write(2, relay2_state);
  EEPROM.write(3, relay3_state);
  EEPROM.write(4, relay4_state);
  EEPROM.commit();
}

// Function to load relay states from EEPROM
void loadRelayStates() {
  relay1_state = EEPROM.read(1);
  relay2_state = EEPROM.read(2);
  relay3_state = EEPROM.read(3);
  relay4_state = EEPROM.read(4);
  previousSwitch1States = EEPROM.read(1);
  previousSwitch2States = EEPROM.read(2);
  previousSwitch3States = EEPROM.read(3);
  previousSwitch4States = EEPROM.read(4);
  digitalWrite(relayPin1, relay1_state ? LOW : HIGH);
  digitalWrite(relayPin2, relay2_state ? LOW : HIGH);
  digitalWrite(relayPin3, relay3_state ? LOW : HIGH);
  digitalWrite(relayPin4, relay4_state ? LOW : HIGH);
  Serial.println("Relay states loaded from EEPROM.");
}


// Function to read data from the DHT sensor
void readDHTData() {

  float humidityValue = dht.readHumidity();
  float temperatureValue = dht.readTemperature();

  // Check if the readings are valid
  if (isnan(humidityValue) || isnan(temperatureValue)) {
    Serial.println("Failed to read from DHT sensor.");
    return;
  }

  // Update RainMaker parameters
  temperature.updateAndReportParam("Temperature", temperatureValue);
  humidity.updateAndReportParam("Temperature", humidityValue);

  Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temperatureValue, humidityValue);
}

// Function to handle switch toggling
void handleSwitch() {
  unsigned long currentTime = millis();
  bool currentSwitch1State = digitalRead(SwitchPin1) == LOW;
  bool currentSwitch2State = digitalRead(SwitchPin2) == LOW;
  bool currentSwitch3State = digitalRead(SwitchPin3) == LOW;
  bool currentSwitch4State = digitalRead(SwitchPin4) == LOW;

  if (previousSwitch1States != currentSwitch1State && (currentTime - lastToggleTimes1 >= debounceDelay)) {
    relay1_state = !relay1_state;                                           // Toggle relay state
    digitalWrite(relayPin1, relay1_state ? LOW : HIGH);                     // Update relay
    my_switch1.updateAndReportParam("Power", relay1_state ? true : false);  // Report the relay state to RainMaker (synchronizing switch state)
    saveRelayStates();                                                      // Save updated state to EEPROM
    lastToggleTimes1 = currentTime;                                         // Update debounce timer
    previousSwitch1States = currentSwitch1State;                            // Update previous state
    relay1_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);
  }
  if (previousSwitch2States != currentSwitch2State && (currentTime - lastToggleTimes2 >= debounceDelay)) {
    relay2_state = !relay2_state;                                           // Toggle relay state
    digitalWrite(relayPin2, relay2_state ? LOW : HIGH);                     // Update relay
    my_switch2.updateAndReportParam("Power", relay2_state ? true : false);  // Report the relay state to RainMaker (synchronizing switch state)
    saveRelayStates();                                                      // Save updated state to EEPROM
    lastToggleTimes2 = currentTime;                                         // Update debounce timer
    previousSwitch2States = currentSwitch2State;                            // Update previous state
    relay2_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);
  }
  if (previousSwitch3States != currentSwitch3State && (currentTime - lastToggleTimes3 >= debounceDelay)) {
    relay3_state = !relay3_state;                                           // Toggle relay state
    digitalWrite(relayPin3, relay3_state ? LOW : HIGH);                     // Update relay
    my_switch3.updateAndReportParam("Power", relay3_state ? true : false);  // Report the relay state to RainMaker (synchronizing switch state)
    saveRelayStates();                                                      // Save updated state to EEPROM
    lastToggleTimes3 = currentTime;                                         // Update debounce timer
    previousSwitch3States = currentSwitch3State;                            // Update previous state
    relay3_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);
  }
  if (previousSwitch4States != currentSwitch4State && (currentTime - lastToggleTimes4 >= debounceDelay)) {
    relay4_state = !relay4_state;                                           // Toggle relay state
    digitalWrite(relayPin4, relay4_state ? LOW : HIGH);                     // Update relay
    my_switch4.updateAndReportParam("Power", relay4_state ? true : false);  // Report the relay state to RainMaker (synchronizing switch state)
    saveRelayStates();                                                      // Save updated state to EEPROM
    lastToggleTimes4 = currentTime;                                         // Update debounce timer
    previousSwitch4States = currentSwitch4State;                            // Update previous state
    relay4_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);
  }


}




// RainMaker write callback to control relays
void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();

  if (strcmp(device_name, deviceName_1) == 0 && strcmp(param_name, "Power") == 0) {
      relay1_state = val.val.b;
      digitalWrite(relayPin1, !relay1_state);
      saveRelayStates();
      relay1_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300); 
      param->updateAndReport(val);
  }
  if (strcmp(device_name, deviceName_2) == 0 && strcmp(param_name, "Power") == 0) {
      relay2_state = val.val.b;
      digitalWrite(relayPin2, !relay2_state);
      saveRelayStates();
      relay2_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);
      param->updateAndReport(val);
  }
  if (strcmp(device_name, deviceName_3) == 0 && strcmp(param_name, "Power") == 0) {
      relay3_state = val.val.b;
      digitalWrite(relayPin3, !relay3_state);
      saveRelayStates();
      relay3_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);
      param->updateAndReport(val);
  }
  if (strcmp(device_name, deviceName_4) == 0 && strcmp(param_name, "Power") == 0) {
      relay4_state = val.val.b;
      digitalWrite(relayPin4, !relay4_state);
      saveRelayStates();
      relay4_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);
      param->updateAndReport(val);
  }


  if (strcmp(param_name, deviceName_1) == 0) {
      if (strcmp(device_name, "Set Remote") == 0) {
          irSetRelay = 1;
          isInIRSetMode = true;
      } else if (strcmp(device_name, "Unset Remote") == 0) {
          EEPROM.put(10,1234567890);
          EEPROM.commit();
          isInIRSetMode = false;
      }
  }
  if (strcmp(param_name, deviceName_2) == 0) {
      if (strcmp(device_name, "Set Remote") == 0) {
          irSetRelay = 2;
          isInIRSetMode = true;
      } else if (strcmp(device_name, "Unset Remote") == 0) {
          EEPROM.put(20,1234567890);
          EEPROM.commit();
          isInIRSetMode = false;
      }
  }
  if (strcmp(param_name, deviceName_3) == 0) {
      if (strcmp(device_name, "Set Remote") == 0) {
          irSetRelay = 3;
          isInIRSetMode = true;
      } else if (strcmp(device_name, "Unset Remote") == 0) {
          EEPROM.put(30,1234567890);
          EEPROM.commit();
          isInIRSetMode = false;
      }
  }
  if (strcmp(param_name, deviceName_4) == 0) {
      if (strcmp(device_name, "Set Remote") == 0) {
          irSetRelay = 4;
          isInIRSetMode = true;
      } else if (strcmp(device_name, "Unset Remote") == 0) {
          EEPROM.put(40,1234567890);
          EEPROM.commit();
          isInIRSetMode = false;
      }
  }
  if (strcmp(param_name, "All") == 0) {
      if (strcmp(device_name, "Set Remote") == 0) {
          irSetRelay = 5;
          isInIRSetMode = true;
      } else if (strcmp(device_name, "Unset Remote") == 0) {
          EEPROM.put(50,1234567890);
          EEPROM.commit();
          isInIRSetMode = false;
      }
  }
}


void handleIRSignal() {
  if (isInIRSetMode) return;

  if (IrReceiver.decode()) {
    uint32_t receivedCode = IrReceiver.decodedIRData.decodedRawData;
    uint32_t irCodes1;
    uint32_t irCodes2;
    uint32_t irCodes3;
    uint32_t irCodes4;
    uint32_t irCodes5;
    
    EEPROM.get(10, irCodes1);
    EEPROM.get(20, irCodes2);
    EEPROM.get(30, irCodes3);
    EEPROM.get(40, irCodes4);
    EEPROM.get(50, irCodes5);

    if (receivedCode == irCodes1) {
      relay1_state = !relay1_state;                                           // Toggle relay state
      digitalWrite(relayPin1, relay1_state ? LOW : HIGH);                     // Update relay
      my_switch1.updateAndReportParam("Power", relay1_state ? true : false);  // Report the relay state to RainMaker (synchronizing switch state)
      saveRelayStates();   
      relay1_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);                                                   // Save updated state to EEPROM
    } else if (receivedCode == irCodes2) {
      relay2_state = !relay2_state;                                           // Toggle relay state
      digitalWrite(relayPin2, relay2_state ? LOW : HIGH);                     // Update relay
      my_switch2.updateAndReportParam("Power", relay2_state ? true : false);  // Report the relay state to RainMaker (synchronizing switch state)
      saveRelayStates();  
      relay2_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);  
    } else if (receivedCode == irCodes3) {
      relay3_state = !relay3_state;                                           // Toggle relay state
      digitalWrite(relayPin3, relay3_state ? LOW : HIGH);                     // Update relay
      my_switch3.updateAndReportParam("Power", relay3_state ? true : false);  // Report the relay state to RainMaker (synchronizing switch state)
      saveRelayStates();
      relay3_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);  
    } else if (receivedCode == irCodes4) {
      relay4_state = !relay4_state;                                           // Toggle relay state
      digitalWrite(relayPin4, relay4_state ? LOW : HIGH);                     // Update relay
      my_switch4.updateAndReportParam("Power", relay4_state ? true : false);  // Report the relay state to RainMaker (synchronizing switch state)
      saveRelayStates(); 
      relay4_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);    
    } else if (receivedCode == irCodes5) {
      bool any_state = relay1_state || relay2_state || relay3_state || relay4_state;
        if (any_state) {
          relay1_state = false;
          relay2_state = false;
          relay3_state = false;
          relay4_state = false;          
        } else {
          relay1_state = true;
          relay2_state = true;
          relay3_state = true;
          relay4_state = true;
        }
        digitalWrite(relayPin1, relay1_state ? LOW : HIGH);
        digitalWrite(relayPin2, relay2_state ? LOW : HIGH);
        digitalWrite(relayPin3, relay3_state ? LOW : HIGH);
        digitalWrite(relayPin4, relay4_state ? LOW : HIGH);
        my_switch1.updateAndReportParam("Power", relay1_state ? true : false);
        my_switch2.updateAndReportParam("Power", relay2_state ? true : false);
        my_switch3.updateAndReportParam("Power", relay3_state ? true : false);
        my_switch4.updateAndReportParam("Power", relay4_state ? true : false);
        saveRelayStates(); 
        any_state ? playbeep(2, 1500, 200) : playbeep(1, 1500, 300);   
    }

    IrReceiver.resume();
  }

}
void handleIRSetMode() {
  if (!isInIRSetMode) return;
  playbeep(4, 1500, 200);

   if (IrReceiver.decode()) {
    uint32_t receivedCode = IrReceiver.decodedIRData.decodedRawData;

    switch (irSetRelay) {
      case 1:
        EEPROM.put(10, receivedCode);
        EEPROM.commit();
        playbeep(1, 1500, 400);
        break;
      case 2:
        EEPROM.put(20, receivedCode);
        EEPROM.commit();
        playbeep(1, 1500, 400);
        break;
      case 3:
        EEPROM.put(30, receivedCode);
        EEPROM.commit();
        playbeep(1, 1500, 400);
        break;
      case 4:
        EEPROM.put(40, receivedCode);
        EEPROM.commit();
        playbeep(1, 1500, 400);
        break;
      case 5:
        EEPROM.put(50, receivedCode);
        EEPROM.commit();
        playbeep(1, 1500, 400);
        break;
    }
    
    irSetRelay = -1;
    isInIRSetMode = false;

    IrReceiver.resume();
   }
}

// Setup function
void setup() {
  Serial.begin(115200);
  EEPROM.begin(512); // Initialize EEPROM with a size of 512 bytes
  
  // Initialize relay pins
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(relayPin3, OUTPUT);
  pinMode(relayPin4, OUTPUT);

  // Initialize switch pins
  pinMode(SwitchPin1, INPUT_PULLUP);
  pinMode(SwitchPin2, INPUT_PULLUP);
  pinMode(SwitchPin3, INPUT_PULLUP);
  pinMode(SwitchPin4, INPUT_PULLUP);

  // Initialize DHT sensor
  dht.begin();
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);


  // Load saved relay states
  loadRelayStates();

  Node my_node;
  my_node = RMaker.initNode(RoomName);


  Param ir1_set_enable(deviceName_1, "custom.param.enable", value(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
  ir1_set_enable.addUIType("esp.ui.trigger");
  setir.addParam(ir1_set_enable);

  Param ir2_set_enable(deviceName_2, "custom.param.enable", value(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
  ir2_set_enable.addUIType("esp.ui.trigger");
  setir.addParam(ir2_set_enable);

  Param ir3_set_enable(deviceName_3, "custom.param.enable", value(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
  ir3_set_enable.addUIType("esp.ui.trigger");
  setir.addParam(ir3_set_enable);

  Param ir4_set_enable(deviceName_4, "custom.param.enable", value(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
  ir4_set_enable.addUIType("esp.ui.trigger");
  setir.addParam(ir4_set_enable);

  Param all_set_enable("All", "custom.param.enable", value(true), PROP_FLAG_READ | PROP_FLAG_WRITE);
  all_set_enable.addUIType("esp.ui.trigger");
  setir.addParam(all_set_enable);

  Param ir1_unset_enable(deviceName_1, "custom.param.enable", value(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
  ir1_unset_enable.addUIType("esp.ui.trigger");
  unsetir.addParam(ir1_unset_enable);

  Param ir2_unset_enable(deviceName_2, "custom.param.enable", value(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
  ir2_unset_enable.addUIType("esp.ui.trigger");
  unsetir.addParam(ir2_unset_enable);

  Param ir3_unset_enable(deviceName_3, "custom.param.enable", value(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
  ir3_unset_enable.addUIType("esp.ui.trigger");
  unsetir.addParam(ir3_unset_enable);

  Param ir4_unset_enable(deviceName_4, "custom.param.enable", value(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
  ir4_unset_enable.addUIType("esp.ui.trigger");
  unsetir.addParam(ir4_unset_enable);

  Param all_unset_enable("All", "custom.param.enable", value(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
  all_unset_enable.addUIType("esp.ui.trigger");
  unsetir.addParam(all_unset_enable);

  // RainMaker Setup
  my_switch1.addCb(write_callback);
  my_switch2.addCb(write_callback);
  my_switch3.addCb(write_callback);
  my_switch4.addCb(write_callback);

  setir.addCb(write_callback);
  unsetir.addCb(write_callback);

  // Add devices to RainMaker  
  my_node.addDevice(temperature);
  my_node.addDevice(humidity);
  my_node.addDevice(my_switch1);
  my_node.addDevice(my_switch2);
  my_node.addDevice(my_switch3);
  my_node.addDevice(my_switch4);

  my_node.addDevice(setir);
  my_node.addDevice(unsetir);

   //This is optional
  RMaker.enableOTA(OTA_USING_PARAMS);
  RMaker.enableTZService();
  RMaker.enableSchedule();

  // Synchronize state from hardware to RainMaker
  my_switch1.updateAndReportParam("Power", relay1_state ? true : false);
  my_switch2.updateAndReportParam("Power", relay2_state ? true : false);
  my_switch3.updateAndReportParam("Power", relay3_state ? true : false);
  my_switch4.updateAndReportParam("Power", relay4_state ? true : false);

  // Start RainMaker
  RMaker.start();

  WiFi.onEvent(sysProvEvent);

  #if CONFIG_IDF_TARGET_ESP32
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);
  #else
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
  #endif

  Serial.println("Setup Complete");
}


// Loop function
void loop() {
  handleSwitch();
  handleIRSignal();
  handleIRSetMode();

  // Periodically read DHT sensor data
  static unsigned long lastDHTRead = 0;
  if (millis() - lastDHTRead > 10000) { // Read every 10 seconds
    lastDHTRead = millis();
    readDHTData();
  }

  if (digitalRead(RESETPIN) == LOW) { //Push button pressed

    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(RESETPIN) == LOW) delay(50);
    int endTime = millis();

    if ((endTime - startTime) > 10000) {
      // If key pressed for more than 10secs, reset all
      Serial.printf("Reset to factory.\n");
      RMakerFactoryReset(2);
    } else if ((endTime - startTime) > 3000) {
      Serial.printf("Reset Wi-Fi.\n");
      // If key pressed for more than 3secs, but less than 10, reset Wi-Fi
      RMakerWiFiReset(2);
    } else {
      // Toggle device state??
    }
  }
  delay(100);

}
