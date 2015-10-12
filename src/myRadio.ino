#include <SPI.h>
#include <Gamebuino.h>
#include <Wire.h>
#include "Bitmaps.h"
#include "pty.h"

#define SEARCH_UP 1
#define SEARCH_DOWN 2
#define SEARCH_NONE 0
#define MONO false
#define STEREO true
#define RDA5807SEQ 0x10
#define RDA5807RAN 0x11

Gamebuino gb;

extern const byte font3x5[]; //a small but efficient font (default)
extern const byte font5x7[]; //a large, comfy font

String rdsCur  = "1234567890123456789012345678901234567890123456789012345678901234";
char rdsFlag = ' ';  //Group 2A, Block2, Bit4
String rdsLast = "no data available";
String rdsProg = "unknown";
char actPTY = 0;
String rdsClock = "hh:mm";
bool mode = MONO; //Stereo
bool trafficOn  = false;
bool trafficAva = false;
bool music = false;
bool hasRDS = false;
char searchMode = SEARCH_NONE;
String rdsAF = "             ";

// mapping band -> frequencies US/EUR JAP    World  EastEUR
float freqMin[4]  = {87.005, 76.005, 76.005, 65.005,};
float freqMax[4]  = {108.00, 91.00, 108.000, 76.000,};
// Mapping bits->spacing (in MHz)
const float space[]  = {0.1, 0.2, 0.05, 0.025,};

// mapping band -> preferred spacing
const char bandspace[]  = {0, 0, 0, 3,};

byte preset = 0; // US/Europe

float  freqIni = 104.30;           // Startfrequenz (Landeswelle)
float  freqAct;

char    volAct = 7;              // Lautstaerke(Volume)
byte    rssiLevel = 0;              // Signal-Level
//int    i_ret = 0;

unsigned int rdsData[32];

//boolean b_debug=false;

unsigned int    RDA5807_Default[10] = {
  0x0758,  // 00 defaultid
  0x0000,  // 01 not used
  0xD009,  // 02 DHIZ,DMUTE,BASS, POWERUPENABLE,RDS
  0x0000,  // 03
  0x0200,//0x1400,  // 04 DE ? SOFTMUTE
  0x84DF,  // 05 INT_MODE,SEEKTH=0110,????, Volume=15
  0x4000,  // 06 OPENMODE=01
  0x0000,  // 07 unused ?
  0x0000,  // 08 unused ?
  0x0000   // 09 unused ?
};
unsigned int    RDA5807_Reg[32];


void setup() {
  gb.begin();
  //Serial.begin(9600);                     // Serial Config
  gb.titleScreen(F("My Radio"));
  Wire.begin();                           // Intialisierung I2C-Bus(2 Wire)
  delay(200);
  RDA5807_Reset();
  delay(200);
  RDA5807_PowerOn();
  delay(200);
  freqAct = freqIni;
  RDA5807_setFreq(freqAct);
  gb.battery.show = true;
}

void loop() {
  if (gb.update()) {
    if (gb.buttons.repeat(BTN_RIGHT, 4)) {
      searchMode = SEARCH_UP;
    }
    if (gb.buttons.repeat(BTN_LEFT, 4)) {
      searchMode = SEARCH_DOWN;
    }
    if (gb.buttons.repeat(BTN_UP, 4)) {
      //Volup
      RDA5807_setVol(++volAct);
    }
    if (gb.buttons.repeat(BTN_DOWN, 4)) {
      //VolDown
      RDA5807_setVol(--volAct);
    }
    if (gb.buttons.repeat(BTN_C, 4)) {
      gb.titleScreen(F("My Radio"));
      RDA5807_Reset();
      delay(200);
      RDA5807_PowerOn();
      delay(200);
      RDA5807_setFreq(freqAct);
    }
    if (gb.buttons.repeat(BTN_A, 4)) {
      searchMode = SEARCH_NONE;
    }
    if (gb.buttons.repeat(BTN_B, 4)) {
      //mute
      RDA5807_toggleMute();
    }
    if (searchMode != SEARCH_NONE) {
      if (searchMode == SEARCH_UP) {
        freqAct += (space[preset] + 0.005);
        if (freqAct > freqMax[preset]) freqAct = freqMin[preset];

      }
      if (searchMode == SEARCH_DOWN) {
        freqAct -= space[preset];
        if (freqAct < freqMin[preset]) freqAct = freqMax[preset];
      }
      RDA5807_setFreq(freqAct);
      delay(100);

    }
    gb.display.cursorX = 7;
    gb.display.cursorY = 0;
    gb.display.print(freqAct);
    gb.display.print(F("MHz"));

    if (mode) gb.display.drawBitmap(0, 0, stereo);
    gb.display.drawBitmap(45, 0, vols[volAct]);
    gb.display.cursorX = 60;
    gb.display.cursorY = 0;
    gb.display.print(rdsClock);
    gb.display.cursorX = 0;
    gb.display.cursorY = 15;
    gb.display.setFont(font5x7);
    gb.display.print(rdsProg);
    gb.display.setFont(font3x5);
    gb.display.cursorX = 0;
    gb.display.cursorY = 30;
    gb.display.print(rdsLast);
    // gb.display.print(rdsCur);

    gb.display.cursorX = 0;
    gb.display.cursorY = 6;
    //music always announced
    if (music) {
      gb.display.print(F("\16 "));
    } else {
      gb.display.print(F("  "));
    }
    //   if (trafficAva){gb.display.print("TA ");}
    if (trafficOn) {
      gb.display.print(F("TP "));
    }
    if (actPTY < 16) {
      strcpy_P(buffer, (char*)pgm_read_word(&(PTY[actPTY])));
      gb.display.print(buffer);
    }
   //  gb.display.print(String(RDA5807_Reg[2],HEX));

    if (rssiLevel < 27) {
      gb.display.drawBitmap(54, 0, rssi[0]);
    } else if (rssiLevel > 35) {
      gb.display.drawBitmap(54, 0, rssi[8]);
    } else {
      gb.display.drawBitmap(54, 0, rssi[rssiLevel - 27]);
    }

    RDA5807_Status();
    if ((searchMode != SEARCH_NONE) && (rssiLevel > 25)) {
      searchMode = SEARCH_NONE;

    }
    if (hasRDS) {
      RDA5807_RDS();
    }
  }
}
