// **********************************************************************************
// This sketch is the base code loaded onto the RCMH Wireless Antler Hat nodes.
// Its purpose is to:
// - Manually update the NODEID parameter, compile, and upload to an individual node.
// - Initialize the EEPROM and save the given config settings into the EEPROM.
// - Sit and wait for further OTA updates. Upon receiving an OTA update the node reloads
// and this sketch is no more.
//
// As currently written, this sketch will OVERWRITE any previously saved configs, it 
// intentionally does not check for an existing config.
// OTA updates will retain their config settings (if so desired and appropriately
// programmed).
// 
// **********************************************************************************
// Change Log
// **********************************************************************************
// 2021-10-12 - Added writing config to EEPROM
//
// **********************************************************************************
// Initial code Copyright Felix Rusu 2020, http://www.LowPowerLab.com/contact
// Further revisions Copyright 2021 Madison Square Garden Entertainment
// contact: Michael Sauder - michael.sauder@msg.com
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************
#include <RFM69.h>         //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_OTA.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h>      //get it here: https://github.com/lowpowerlab/spiflash
#include <EEPROMex.h>      //get it here: http://playground.arduino.cc/Code/EEPROMex

//****************************************************************************************************************
//**** IMPORTANT RADIO SETTINGS - YOU MUST CHANGE/CONFIGURE TO MATCH YOUR HARDWARE TRANSCEIVER CONFIGURATION! ****
//****************************************************************************************************************
#define NODEID        203  // node ID used for this unit
#define NETWORKID     150
#define GATEWAYID1     1    // node ID for the default gateway.
#define GATEWAYID2     2    // node ID for the default gateway.
#define FREQUENCY     RF69_915MHZ
#define FREQUENCY_EXACT 905500000
#define ENCRYPTKEY "rcmhprodrcmhprod" //16-bytes or ""/0/null for no encryption
//String ENCRYPTKEYSTRING = "rcmhprodrcmhprod";
#define IS_RFM69HW_HCW true //uncomment only for RFM69HW/HCW! Leave out if you have RFM69W/CW!
//*****************************************************************************************************************************
#define ENABLE_ATC    //comment out this line to disable AUTO TRANSMISSION CONTROL
#define ATC_RSSI      -80
#define FLASH_ID      0xEF30  //ex. 0xEF30 for windbond 4mbit, 0xEF40 for windbond 16/64mbit
//*****************************************************************************************************************************
//#define BR_300KBPS         //run radio at max rate of 300kbps! Probably never want this.
//*****************************************************************************************************************************
#define SERIAL_BAUD 115200
#define BLINKPERIOD 250
//*****************************************************************************************************************************
SPIFlash flash(SS_FLASHMEM, FLASH_ID);

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif


char input = 0;
long lastPeriod = -1;


// struct for EEPROM config
struct configuration {
  byte codeVersion; // What version code we're using
  byte frequency; // What family are we working in? Basically always going to be 915Mhz in RCMH.
  long frequency_exact; // The exact frequency we're operating at.
  bool isHW; // Is this a high power radio?
  byte nodeID;    // 8bit address (up to 255)
  byte networkID; // 8bit address (up to 255)
  byte gatewayID1; // 8bit address (up to 255)
  byte gatewayID2; // 8bit address (up to 255)
  char encryptionKey[16];
  int state;     // Just in case we want to save a state setting.
  
} CONFIG;

void setup() {
  EEPROM.setMaxAllowedWrites(10000);
  EEPROM.readBlock(0, CONFIG); pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(SERIAL_BAUD);
  delay(1000);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.encrypt(ENCRYPTKEY); //OPTIONAL

#ifdef FREQUENCY_EXACT
  radio.setFrequency(FREQUENCY_EXACT); //set frequency to some custom frequency
#endif

#ifdef ENABLE_ATC
  radio.enableAutoPower(ATC_RSSI);
#endif

#ifdef IS_RFM69HW_HCW
  radio.setHighPower(); //must include this only for RFM69HW/HCW!
#endif
  Serial.println("Start node...");
  Serial.print("Node ID = ");
  Serial.println(NODEID);

  if (flash.initialize())
    Serial.println("SPI Flash Init OK!");
  else
    Serial.println("SPI Flash Init FAIL!");

#ifdef BR_300KBPS
  radio.writeReg(0x03, 0x00);  //REG_BITRATEMSB: 300kbps (0x006B, see DS p20)
  radio.writeReg(0x04, 0x6B);  //REG_BITRATELSB: 300kbps (0x006B, see DS p20)
  radio.writeReg(0x19, 0x40);  //REG_RXBW: 500kHz
  radio.writeReg(0x1A, 0x80);  //REG_AFCBW: 500kHz
  radio.writeReg(0x05, 0x13);  //REG_FDEVMSB: 300khz (0x1333)
  radio.writeReg(0x06, 0x33);  //REG_FDEVLSB: 300khz (0x1333)
  radio.writeReg(0x29, 240);   //set REG_RSSITHRESH to -120dBm
#endif

// Program the EEPROM

CONFIG.frequency=FREQUENCY;
CONFIG.frequency_exact=FREQUENCY_EXACT;
CONFIG.isHW=IS_RFM69HW_HCW;
CONFIG.nodeID=NODEID;
CONFIG.networkID=NETWORKID;
CONFIG.gatewayID1=GATEWAYID1;
CONFIG.gatewayID2=GATEWAYID2;
// Having some stupid problem related to the rfm69 library requiring the "ENCRYPTKEY" to be a const char array,
// having trouble converting, AND having trouble with a loop for some reason. Am newbie. So eff it, brute forcing.
CONFIG.encryptionKey[0]=ENCRYPTKEY[0];
CONFIG.encryptionKey[1]=ENCRYPTKEY[1];
CONFIG.encryptionKey[2]=ENCRYPTKEY[2];
CONFIG.encryptionKey[3]=ENCRYPTKEY[3];
CONFIG.encryptionKey[4]=ENCRYPTKEY[4];
CONFIG.encryptionKey[5]=ENCRYPTKEY[5];
CONFIG.encryptionKey[6]=ENCRYPTKEY[6];
CONFIG.encryptionKey[7]=ENCRYPTKEY[7];
CONFIG.encryptionKey[8]=ENCRYPTKEY[8];
CONFIG.encryptionKey[9]=ENCRYPTKEY[9];
CONFIG.encryptionKey[10]=ENCRYPTKEY[10];
CONFIG.encryptionKey[11]=ENCRYPTKEY[11];
CONFIG.encryptionKey[12]=ENCRYPTKEY[12];
CONFIG.encryptionKey[13]=ENCRYPTKEY[13];
CONFIG.encryptionKey[14]=ENCRYPTKEY[14];
CONFIG.encryptionKey[15]=ENCRYPTKEY[15];

EEPROM.writeBlock(0,CONFIG);
Serial.println("CONFIG saved to EEPROM");

}

void loop(){
  // This part is optional, useful for some debugging.
  // Handle serial input (to allow basic DEBUGGING of FLASH chip)
  // ie: display first 256 bytes in FLASH, erase chip, write bytes at first 10 positions, etc
  if (Serial.available() > 0) {
    input = Serial.read();
    if (input == 'd') //d=dump first page
    {
      Serial.println("Flash content:");
      int counter = 0;

      while(counter<=256){
        Serial.print(flash.readByte(counter++), HEX);
        Serial.print('.');
      }
      
      Serial.println();
    }
    else if (input == 'D') //d=dump higher memory
    {
      Serial.println("Flash content:");
      uint16_t counter = 4090; //dump the memory between the first 4K and second 4K sectors

      while(counter<=4200){
        Serial.print(flash.readByte(counter++), HEX);
        Serial.print('.');
      }
      
      Serial.println();
    }
    else if (input == 'e')
    {
      Serial.print("Erasing Flash chip ... ");
      flash.chipErase();
      while(flash.busy());
      Serial.println("DONE");
    }
    else if (input == 'i')
    {
      Serial.print("DeviceID: ");
      Serial.println(flash.readDeviceId(), HEX);
    }
    else if (input == 'r')
    {
      Serial.print("Rebooting");
      resetUsingWatchdog(true);
    }
    else if (input == 'R')
    {
      Serial.print("RFM69 registers:");
      radio.readAllRegs();
    }
  }
  
  // Check for existing RF data, potentially for a new sketch wireless upload

  if (radio.receiveDone())
  {
    Serial.print("Got [");
    Serial.print(radio.SENDERID);
    Serial.print(':');
    Serial.print(radio.DATALEN);
    Serial.print("] > ");
    for (byte i = 0; i < radio.DATALEN; i++)
      Serial.print((char)radio.DATA[i], HEX);
    Serial.println();
    CheckForWirelessHEX(radio, flash, false);
    Serial.println();
  }
  
  //*****************************************************************************************************************************
  // Real sketch code here, let's blink the onboard LED
  if ((int)(millis()/BLINKPERIOD) > lastPeriod)
  {
    lastPeriod++;
    digitalWrite(LED_BUILTIN, lastPeriod%2);
    Serial.print("BLINKPERIOD ");Serial.println(BLINKPERIOD);
  }
  //*****************************************************************************************************************************
}