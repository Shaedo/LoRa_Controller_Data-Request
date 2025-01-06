/* LORA CONTROLLER - DATA REQUEST */

/******* SUMMARY *****
  *PIN-OUT*
    https://resource.heltec.cn/download/Wireless_Stick_Lite_V3/HTIT-WSL_V3.png
    Inbuilt LoRa device, no external connections
*/

/******* INCLUDES ***********/
  #include <Arduino.h>
  #include "lora_config.h"
  #include <RadioLib.h>
  
/******* FUNCTION PROTOTYPES ***********/
  void setup();
  void initLoRa();
  void loop();
  void loRaTX();
  void requestPeripheralReport(byte peripheralID);
  void listenLoRa();
  void parseRx();
  void updatePeripheralReport(); 

/******* GLOBAL VARS ***********/
  #define REPORTING_MODE 2 //0 = prod mode, 1 = include comments, 2 = dev (resests all, eg wipes EEPROM records etc)
  #define SERIAL_BAUD_RATE 115200
    
  #define LoRa_BUFFER 12 // bytes tx/rx each LoRa transmission
  uint8_t loraData[LoRa_BUFFER]; // array of all tx/rx LoRa data

  // Flag for current LoRa cycle - loops around from MIN_CURRENT_CYCLE to 255 
  // Prevents relays from doubling up on requests etc. This step is not neccesarry for simple systems (no relays, no cross transmissions), but has low overhead
  uint8_t currentLoRaCycle_g = 99;

  //Controllers current task....
  enum Status {
      INIT,
      IDLE,
      PERIPHERAL_REPORT,
      LISTENING_PERIPHERAL,
      REPORT_TIMED_OUT
  };
  Status status = INIT;

/******* INSTANTIATED CLASS OBJECTS ***********/
  SX1262 radio = new Module(LoRa_NSS, LoRa_DIO1, LoRa_NRST, LoRa_BUSY); // see lora_config.h

/******* INIT - SETUP ***********/
  void setup(){
    #if REPORTING_MODE > 0
        Serial.begin(SERIAL_BAUD_RATE);
        Serial.println("Initializing ... ");
        delay(500);
    #endif 

    initLoRa();
    delay(500);

    #if REPORTING_MODE > 0
      Serial.println("Setup Complete.");
    #endif
    status = IDLE;
  }
  void initLoRa(){
    #if REPORTING_MODE > 0
      Serial.print(F("[SX1262] Initializing ... "));
    #endif
    int state = radio.begin(LoRa_CARRIER_FREQUENCY,
                            LoRa_BANDWIDTH,
                            LoRa_SPREADING_FACTOR,
                            LoRa_CODING_RATE,
                            LoRa_SYNC_WORD,
                            LoRa_OUTPUT_POWER,
                            LoRa_PREAMBLE_LENGTH );

    if (state == RADIOLIB_ERR_NONE) {
      #if REPORTING_MODE > 0
        Serial.println(F("LoRa Init success!"));
      #endif
    } else {
      #if REPORTING_MODE > 0
        Serial.print(F("LoRa Init failed, code "));
        Serial.println(state);
      #endif
      while (true);
    }
  }
 
/******* LOOP ***********/
    void loop() {
       switch (status) {
        case LISTENING_PERIPHERAL: {
//Serial.println("status.... LISTENING_PERIPHERAL");

          listenLoRa();
          // placed at top of switch to maximise rx expediency
          break;
        }
        case IDLE: {
Serial.println("status.... IDLE");
          status = PERIPHERAL_REPORT;
          break;
        }
        case PERIPHERAL_REPORT: {
Serial.println("status.... PERIPHERAL_REPORT");
          //XXX magic number as stripped down code doesn't account for multiple peripherals   
          requestPeripheralReport(255);
          break;
        }
      }  
    }
  /**LoRa TX - CONTROLLER → PERIPHERAL **/
    void requestPeripheralReport(byte peripheralID) {
      //cycle[0] | type[1] | target ID[2]
      currentLoRaCycle_g ++;
      loraData[0] = currentLoRaCycle_g;
      loraData[1] = REQUESTTYPE_REPORT;
      loraData[2] = peripheralID;

      loRaTX();
    }
    void loRaTX(){
      //attempt to prevent multiple units simulataneously transmitting 
      while(radio.getRSSI(false) > RSSI_INIT_TX_THRESHOLD){
        #if REPORTING_MODE > 0
          Serial.println("RSSI: "+ String(radio.getRSSI(false)));
        #endif
        delay(100);
      }

      //transmit formatted data (eg request to perihperal)
      int SX1262state = radio.transmit(loraData, LoRa_BUFFER);

      #if REPORTING_MODE > 0
        if (SX1262state == RADIOLIB_ERR_NONE) {// the packet was successfully transmitted
          Serial.println("TX success. Datarate: "+ String(radio.getDataRate()) + "bps");
        } else if (SX1262state == RADIOLIB_ERR_PACKET_TOO_LONG) {// the supplied packet was longer than 256 bytes
          Serial.println(F("too long!"));
        } else if (SX1262state == RADIOLIB_ERR_TX_TIMEOUT) {// timeout occured while transmitting packet
          Serial.println(F("timeout!"));
        } else {// some other error occurred
          Serial.println("TX Fail code: "+String(SX1262state));
        }
      #endif
      status = LISTENING_PERIPHERAL;
    }
 
  /** LoRa RX - PERIPHERAL → CONTROLLER **/
    void listenLoRa() {
      int SX1262state = radio.receive(loraData, LoRa_BUFFER);
      if (SX1262state == RADIOLIB_ERR_NONE) {// packet was successfully received
        #if REPORTING_MODE > 0 
          Serial.println(F("Rx success!"));
        #endif
        parseRx();
      }
      #if REPORTING_MODE > 0 
        else if (SX1262state == RADIOLIB_ERR_RX_TIMEOUT) {// timeout occurred while waiting for a packet
        //     Serial.println(F("timeout!"));  // this can get very verbose in normal operations!
        } 
        else if (SX1262state == RADIOLIB_ERR_CRC_MISMATCH) {// packet was received, but is malformed
          Serial.println(F("CRC error!"));
        } 
        else {// some other error occurred
            Serial.print(F("failed, code "));
            Serial.println(SX1262state);
        }
      #endif
    } 
    void parseRx(){

      if(currentLoRaCycle_g != (int)loraData[0]){
        currentLoRaCycle_g = (int)loraData[0];
        
        uint8_t requestType = (int)loraData[1];
        uint8_t source = (int)loraData[2];

        #if REPORTING_MODE > 0
          Serial.println("LoRaData: re "+String(requestType)+ " from:"+ String(source));
          Serial.println("requestType "+ requestType);
          Serial.println("Recieved: "+String(loraData[0])+","+String(loraData[1])+","+String(loraData[2])+","+String(loraData[3])+","+String(loraData[4])+","+String(loraData[5])+","+String(loraData[6])+","+String(loraData[7])+","+String(loraData[8]));
        #endif
        
        switch (requestType) {
          case RETURNTYPE_REPORT:{
            //cycle[0] | type[1] | id[2] | data: rssi[3], battery[4], sensor-data [5...]
            updatePeripheralReport();
            break;
          }
          default:
            #if REPORTING_MODE > 0
              Serial.println("Rx LoRa ReturnType not found");
            #endif
            break;
        }
      }
    } 
    void updatePeripheralReport(){
      //cycle[0] | type[1] | id[2] | rssi[3], battery[4], sensor-data [5...]
      
      uint8_t rssi = -(int)radio.getRSSI(); // having negative numbers for RSSI is annoying; removing the sign reduces storage size and makes more readable
      uint8_t peripheralID = (int)loraData[2];

      uint8_t reportLength = (int)loraData[6];
      String report ="";
      for(uint8_t i = 6; i < reportLength +5; i++){
        char b = loraData[i];
Serial.print("char ");
Serial.print(i);
Serial.print(": ");
Serial.println(b);

        report += b;
      }

      #if REPORTING_MODE > 0
        Serial.println("Report received:");
        Serial.println(report);
      #endif

delay(3000);
status = PERIPHERAL_REPORT;

    }
 
/*END*/
