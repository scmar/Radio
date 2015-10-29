// radio specific definitions



//----------------------------------------
// Write 16Bit To I2C / Two Wire Interface
//----------------------------------------
void Wire_write16(unsigned int val)
{
  Wire.write(val >> 8); Wire.write(val & 0xFF);
}



//----------------------------------------
// RDA5807 Set all Configuration Registers
//----------------------------------------
void RDA5807_Write()
{
  Wire.beginTransmission(RDA5807SEQ);
  for ( int i = 2; i < 7; i++) {
    Wire_write16(RDA5807_Reg[i]);
  }
  Wire.endTransmission();
}



//----------------------------------------
// RDA5807 Set one Configuration Registers
//----------------------------------------
void RDA5807_WriteReg(byte regAdr)
{
  Wire.beginTransmission(RDA5807RAN);
  Wire.write(regAdr);
  Wire_write16(RDA5807_Reg[regAdr]);
  Wire.endTransmission();
}



//---------------------------------------------
// RDA5807 Read Special Data Registers as Word
//---------------------------------------------
void RDA5807_ReadW(byte regCount)
{
  Wire.beginTransmission(RDA5807RAN);
  Wire.write(0x0C);                                // Start at Register 0x0C
  Wire.endTransmission(0);                         // restart condition
  Wire.requestFrom(RDA5807RAN, 2 * regCount, 1);    // Retransmit device address with READ, followed by 2*Count bytes (Count registers)
  for (int i = 0; i < regCount; i++) {
    rdsData[i] = 256 * Wire.read() + Wire.read();   // Read Data into Array of rdsData (int!)
  }
  Wire.endTransmission();
}

void RDA5807_toggleMute() {
  RDA5807_Reg[2] ^= 0x4000;
  RDA5807_WriteReg(2);
}

//--------------------------------------------
// RDA5807 Reset Chip to Default Configuration
//--------------------------------------------
void RDA5807_Reset()
{
  for (byte i = 0; i < 7; i++) {
    RDA5807_Reg[i] = RDA5807_Default[i];
  }
  RDA5807_Reg[2] = RDA5807_Reg[2] | 0x0002; // Enable SoftReset
  RDA5807_Write();
  RDA5807_Reg[2] = RDA5807_Reg[2] & 0xFFFD; // Disable SoftReset //FFFB
}


//----------------------------------------
// RDA5807 Power On
//----------------------------------------
void RDA5807_PowerOn()
{
  RDA5807_Reg[3] = RDA5807_Reg[3] | 0x0010; // Enable Tuning
  RDA5807_Reg[2] = RDA5807_Reg[2] | 0x0001; // Enable PowerOn
  RDA5807_Write();
  RDA5807_Reg[3] = RDA5807_Reg[3] & 0xFFEF; // Disable Tuning
}



//----------------------------------------
// RDA5807 Set Volume
//----------------------------------------
void RDA5807_setVol(char volNew)
{
  if (volNew > 15) {
    volAct = 15;
    return;
  }
  if (volNew < 0)  {
    volAct = 0;
    return;
  }
  volAct = volNew;
  RDA5807_Reg[5] = (RDA5807_Reg[5] & 0xFFF0) | volAct; // Set New Volume
  RDA5807_WriteReg(5);
}

//----------------------------------------
// RDA5807 Tune Radio to Frequency, Clear RDS-Data
//----------------------------------------
void RDA5807_setFreq(float freqNew)
{
  rdsFlag = ' ';
  for (char i = 0; i < 64; i++) {
    rdsCur[i] = ' ';
  }
  rdsProg = "xxx.xMHz";
  actPTY = 0;
  //  rdsAF="";
  freqAct = freqNew;
  rdsProg = ((freqAct < 100) ? " " : "") + String (freqAct, 1) + "MHz";
  int chanNr = (freqNew - freqMin[preset]) / space[preset];
  chanNr = chanNr & 0x03FF;
  RDA5807_Reg[3] = chanNr * 64 + 0x10 + preset * 4 + bandspace[preset];
  Wire.beginTransmission(RDA5807SEQ);
  Wire_write16(RDA5807_Reg[2]);
  Wire_write16(RDA5807_Reg[3]);
  Wire.endTransmission();
  delay(200);                        //little delay to wait finish tuning
  RDA5807_Status();
}


//--------------------------
// RDA5807 Radio Data System
//--------------------------
void RDA5807_RDS() {
  char curFlag = ' ';
  RDA5807_ReadW(4);                              // Read RDS-Data as 4 Word to Array
  if (rdsData[0] != oldPI) {
    oldPI = rdsData[0];
    return; //simple test for stable RDS
  }
  //all Groups
  trafficAva = ((rdsData[1] & 0x0400) == 0x0400) ? true : false;
  actPTY = ((rdsData[1] >> 5) & 0x1F);

  //Group 2 - Radio-Text
  if ((rdsData[1] & 0xF000) == 0x2000) {
    //if rds-textflag toggles: new line of radiotext
    curFlag = ((rdsData[1] & 0x0010) == 0) ? 'A' : 'B';
    if (curFlag != rdsFlag) {
      for (char i = 0; i < 64; i++) {
        rdsLast[i] = rdsCur[i];
        rdsCur[i] = ' ';
      }
      rdsLast[63] = '\0';
      rdsFlag = curFlag;
    }
    char x = 4 * (rdsData[1] & 0x000F);
    for (int i = 2; i < 4; i++) {
      rdsCur[x + (i - 2) * 2] = (char)((rdsData[i] >> 8) & 0xFF);
      rdsCur[x + (i - 2) * 2 + 1] = (char)(rdsData[i] & 0xFF);
    }
  }

  // Group 4a - Clock ua
  if ((rdsData[1] & 0xF800) == 0x4000) {
    int hh = (16 * (rdsData[2] & 0x0001) + ((rdsData[3] & 0xF000) >> 12));
    int mm = (rdsData[3] & 0x0FC0) >> 6;
    int ofs = (rdsData[3] & 0x003F);
    hh += (ofs / 2);
    rdsClock = (hh < 24) ? (((hh < 10) ? " " : "") + String(hh) + ":" + ((mm < 10) ? "0" : "") + String(mm)) : "--:--";
  }

  //Group 0
  if ((rdsData[1] & 0xF000) == 0x0000) {
    //program station name
    char x = 2 * (rdsData[1] & 0x0003); //PS Pos
    rdsProg[x] = (char)((rdsData[3] >> 8) & 0xFF);
    rdsProg[x + 1] = (char)(rdsData[3] & 0xFF);
    //traffic announcement
    trafficOn = ((rdsData[1] & 0x0010) == 0x0010) ? true : false;
    music = ((rdsData[1] & 0x0008) == 0x0008) ? true : false;
    //group 0A
    /*
    if ((rdsData[1]&0x0800)==0x0000){
      byte af[]={((rdsData[2]>>8) & 0xFF),(rdsData[2] & 0xFF),((rdsData[3]>>8) & 0xFF),(rdsData[3] & 0xFF)};
      if (af[0] > 223 && af[0]<250) af[0]-=224;
      rdsAF="";
      for (byte i=0;i<4;i++){
        if (af[i]>0 && af[i]<205) rdsAF+=String(87.50 + af[i]*0.1)+" ";  //elektron-bbs.de - not sure, if its working outside europe (freq & chanspace)
      }
    }
    */
  }
  //Group 8a tmc
  if ((rdsData[1] & 0xF800) == 0x8000) {
    if (((rdsData[1] & 0x0010) == 0) && eventListFound) { //Trafficmarker
      if ( ( (rdsData[1] & 0x0008) == 0x0008) || ((rdsData[2] & 0x8000) == 0x8000)) {
        curEvent = (rdsData[2] & 0x7FF); //11 bit ->2048 Events
        if (curEvent != 0) {
          if (lastEvent != curEvent) {
            //clear TMCData
            byte TMCPos = 0;
            WORD bytesRead;
            for (byte i = 0; i < 64; i++) TMCData[i] = ' ';
            TMCData[63] = '\0';

            //read location
            pf_lseek(32 * ((DWORD)rdsData[3] + 2047 ));
            pf_read((void*)&textBuffer, 32, &bytesRead);
            while ((TMCPos < 63) && (textBuffer[TMCPos] != '\0')) {
              TMCData[TMCPos] = textBuffer[TMCPos];
              TMCPos++;
            }
            TMCData[TMCPos++] = ':';
            //read event
            pf_lseek(32 * (curEvent - 1));
            pf_read((void*)&textBuffer, 32, &bytesRead);
            byte EVPos = 0;
            while ((TMCPos + EVPos < 63) && (textBuffer[EVPos] != '\0')) {
              TMCData[TMCPos + EVPos] = textBuffer[EVPos];
              EVPos++;
            }
            lastEvent = curEvent;
            TMCDuration = TMCDURATION;
          }
        }
      }
    }
  }

}


//----------------------------------------
// RDA5807 Get Statusbytes/Flags
//----------------------------------------
void RDA5807_Status()
{
  int chanNr;
  Wire.requestFrom (RDA5807SEQ, 16);
  for (byte i = 0; i < 8; i++) {
    RDA5807_Reg[0x0A + i] = 256 * Wire.read () + Wire.read();
  }
  Wire.endTransmission();
  chanNr = RDA5807_Reg[0x0A] & 0x03FF;
  rssiLevel = RDA5807_Reg[0x0B] >> 10;
  freqAct = freqMin[preset] + chanNr * space[preset];
  if ((RDA5807_Reg[0x0A] & 0x0400) == 0) {
    mode = MONO;
  } else {
    mode = STEREO;
  }
  if ((RDA5807_Reg[0x0A] & 0x9000) == 0x9000) {
    hasRDS = true;
  }    else {
    hasRDS = false;
  }
}

