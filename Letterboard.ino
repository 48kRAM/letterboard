/*
 An electronic letterboard for use in teaching autistic
 children to communicate. Translates touches on a letter-
 board into USB keyboard keypresses and includes an
 on-board LCD display.

 Copyright 2014 by Joshua Malone (josh.malone@gmail.com)
 
 This code is released under the terms of the GNU
 General Public License, version 2.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 See the LICENSE file for a full copy of the GNU General Public License.
 
 Uses public domain sample code from:
 http://www.arduino.cc/en/Tutorial/LiquidCrystal
 as well as other examples.
 */
 
#include <EEPROM.h>
#include <TouchScreen.h>
#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 13, 9, 4, 5, 6, 7);

#include "lbdefs.h"
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300); 
Point p;

int adc_key_in;          // Value from button ADC
int key=-1;
int oldkey=-1;           // Previously-pressed button
int lcdX=0, lcdY=0;      // Current lcd cursor position
int scrollBack=0;        // How far back in buffer to display
char keyByte=0;          // Character inputted via letterboard
char lcdBuffer[5][17];   // Scrollback buffer for LCD contents
#define LBUF_SIZE 4      // Size of ring buffer, zero-based
int lcdBufPos=0;
char copyBuffer[22];    // For copying const strings from flash memory
int Menu=-1;
#ifdef DEBUG_LB
char debugstr[20];
#endif
prog_char menu_0[] PROGMEM = "Board Type";
prog_char option_0_0[] PROGMEM = "Alpha ABC";
prog_char option_0_1[] PROGMEM = "Numbers";
prog_char option_0_2[] PROGMEM = "Full QWERTY";
prog_char menu_1[] PROGMEM = "Letter Case";
prog_char option_1_0[] PROGMEM = "Upper";
prog_char option_1_1[] PROGMEM = "Lower";
prog_char menu_2[] PROGMEM = "Calibrate";
prog_char option_2_0[] PROGMEM = "Press DN to cal";

uint8_t boardType=0;    // Alpha ABC, by default
uint8_t letterCase=1;    // Lower-case by default
const char *menu_table[] = { menu_0,  menu_1, menu_2  };
#define NUM_MENUS 3
const char *options_0[] = {  option_0_0,  option_0_1,  option_0_2, };
const char *options_1[] = {  option_1_0,  option_1_1, };
#define NUM_CASES 2

#define SW_VERS "ver. 0.71 "



void dispCurrentMenu(int menuNumber) {
    lcd.clear();
    lcd.setCursor(0,0);
    strcpy_P(copyBuffer, (char*)menu_table[Menu] );
    lcd.print(copyBuffer);
    lcd.print(':');
}
void dispCurrentOption(int menuNumber) {
    if(menuNumber >= NUM_MENUS) { return; }
    dispCurrentMenu(menuNumber);
    if(menuNumber==0) {
        /* Select board type */
        strcpy_P(copyBuffer, (char*)options_0[boardType] );
    }
    if(menuNumber==1) {
        /* Select character case */
        strcpy_P(copyBuffer, (char*)options_1[letterCase] );
    }
    if(menuNumber==2) {
        /* Calibrate */
        strcpy_P(copyBuffer, (char*)option_2_0);
    }
    lcd.setCursor(1,1);
    lcd.print(copyBuffer);
}

void changeOption(const int menuNum, int delta) {
    if(menuNum==0) {
        /* Board type */
        boardType=boardType+delta;
        if(boardType<0) { boardType = NUM_BOARDS - 1;  }
        else if(boardType>=NUM_BOARDS) { boardType = 0; }
    }
    if(menuNum==1) {
        /* Char case */
        letterCase=letterCase+delta;
        if(letterCase<0) { letterCase = NUM_CASES - 1; }
        else if (letterCase>=NUM_CASES) { letterCase = 0; }
    }
    if(menuNum==2) {
        /* Calibrate */
        calibrateScreen();
        //saveCalibration(EEPROM_CAL_START);
        lcd.print(F("Complete"));
    }
}

int saveSettings(void) {    /* Returns number of settings saved */
    int retval=0;
    if(boardType != EEPROM.read(EEPROM_BOARD_TYPE) ) {
        retval++;
        EEPROM.write(EEPROM_BOARD_TYPE, boardType);
    }
    if(letterCase != EEPROM.read(EEPROM_LETTER_CASE) ) {
        retval++;
        EEPROM.write(EEPROM_LETTER_CASE, letterCase);
    }
    return(retval);
}
int readCalibration(int ee_offset) {
    unsigned int i;
    byte *p=(byte*)&lb_coords;
    for (i=0; i<sizeof(lb_coords); i++) {
        *p++ = EEPROM.read(ee_offset++);
    }
    return i;
}
int saveCalibration(int ee_offset) {
    unsigned int i;
    const byte *p=(const byte*)&lb_coords;
    for (i=0; i < sizeof(lb_coords); i++) {
        EEPROM.write(ee_offset++, *p++);
    }
    return i;
}
    
void restoreScreen(int scrollback=0) {
    int curLine;
    // Cap scrollback len to size of ring buffer
    if(scrollback > LBUF_SIZE+1) { scrollback = LBUF_SIZE+1; }
    Serial.print(F("SB:  "));
    Serial.println(scrollback);
    /* Print saved text to the LCD screen; optionally
     * scroll back in the buffer a number of lines */
    curLine=lcdBufPos-1-scrollback;
    if(curLine<0) { curLine += LBUF_SIZE+1; }
    else if(curLine>LBUF_SIZE) { curLine-=LBUF_SIZE +1; }
    Serial.print(F("Ring Buffer: "));
    Serial.println(curLine);
    lcd.clear();
    lcd.setCursor(0,0);    // Restore top line
    lcd.print(lcdBuffer[curLine++]);
    lcd.setCursor(0,1);    // Restore bottom line
    lcd.print(lcdBuffer[curLine]);
}
void calibrateScreen() {
    int j;
    uint8_t direction, i;
    lcd.setCursor(0,0);
    lcd.print(F("Calibration"));
    for (i=0; i<board_dims[boardType][DIM_Y]; i++) {
        lcd.setCursor(0,1);
        lcd.print(F("Press "));
        lcd.print(getBoardSymbol(0,i) );
        p.x=0;
        while(p.x < 30) {
            p=ts.getPoint();    // Keep reading until we get a point
        }
        // Note: touchscreen library already oversamples for us so there's no need
        //   to oversample during calibration like we normally would
        lb_coords[boardType][DIM_Y][i]=p.y;
        if(i==0) {
            lb_coords[boardType][DIM_X][i]=p.x;
        }
        if(i>0) {
            // Determine midpoint and save *that* as the cal value for prev point
            j=p.x+lb_coords[boardType][DIM_X][i-1];
            lb_coords[boardType][DIM_X][i-1]=j/2;
            j=p.y+lb_coords[boardType][DIM_Y][i-1];
            lb_coords[boardType][DIM_Y][i-1]=j/2;
        }
        while(p.x>30) {
            // Wait until finger is lifted
            p=ts.getPoint();
        }
        delay(200);
    }
#ifdef DEBUG_LB
    Serial.println(F("Cal results:\n"));
    for(i=0; i<6; i++) {
        sprintf(debugstr, "%d ", lb_coords[boardType][DIM_X][i] );
        Serial.print(debugstr);
    }
    Serial.print("\n");
    for(i=0; i<6; i++) {
        sprintf(debugstr, "%d ", lb_coords[boardType][DIM_Y][i] );
        Serial.print(debugstr);
    }
    Serial.print("\n");
#endif
    delay(300);
    lcd.clear();
    lcd.setCursor(0,0);
}
        
char getBoardSymbol(int x, int y) {
    char keyByte=0;
    if(boardType==BOARD_ABC) {
        if(x>4) {
            keyByte=alphaColumnSix[y];
        } else {
            keyByte=ASCII_A + (letterCase*32) + (x+y*(LB_ALPHA_X-1) );
        }
    } else if(boardType==BOARD_NUM) {
        if(y==0) {
            // Line 1 - operators
            if(x>5) { Serial.println(F("OOB error decoding number board\n")); }
            keyByte=boardNumOperators[x];
        } else {
            keyByte=ASCII_0 + (x+(y-1)*LB_NUM_X);
        }               
    }
    return(keyByte);
}
/************************************************************/

void loop() {
    int x, y;
    uint8_t board_x, board_y;
    key = get_key();
    if (key != oldkey) {
        oldkey = key;
        if(key == BUTTON_LEFT) {
            /* Enter Menu mode or advance to next menu */
            Menu++;
            if(Menu >= NUM_MENUS) { Menu=0; }
            /* Displaying option has side-effect of displaying
             *  the menu name, too.  This works for us here. */
            dispCurrentOption(Menu);
        }
        if(Menu>=0) {
            /* Other buttons only work in menu mode */
            if(key == BUTTON_DOWN) {
                /* Alter option value */
                changeOption(Menu, -1);
                dispCurrentOption(Menu);
            }
            if(key == BUTTON_UP) {
                changeOption(Menu, 1);
                dispCurrentOption(Menu);
            }
            if(key == BUTTON_RIGHT) {
                /* Exit menu mode */
                Menu = -1;
                /* Save menu settings */
                Serial.print(F("Saving settings. Changed: "));
                Serial.println(saveSettings() );
                /* Restore previous typing */
                restoreScreen();
            }
        } else {
            if(key == BUTTON_SELECT) {
                /* Clear screen and buffer */
                lcdBufPos=0;
                lcdX=0;
                lcd.clear();
                lcd.setCursor(0,1);
            }
            if(key == BUTTON_UP) {     // Scrollback
                restoreScreen(++scrollBack);
            }
            if(key == BUTTON_DOWN && scrollBack >0) {
                restoreScreen(--scrollBack);
            }
        }
    }
    p = ts.getPoint();

    // Translate touch coordinates into character
    if(p.x > 30 && keyByte==0 && Menu < 0) {   
#ifdef DEBUG_LB
        sprintf(debugstr, "Tap %d,%d\n", p.x, p.y);
        Serial.println(debugstr);
#endif        
        board_x=board_dims[boardType][0];
        board_y=board_dims[boardType][1];
        if(scrollBack>0) {
            restoreScreen();
            scrollBack=0;
        }
        // Decode the character from the touch panel
        for (x=0; x < board_x; x++) {
            if(p.x < lb_coords[boardType][DIM_X][x]) { break; }
        }
        for (y=0; y < board_y; y++) {
            if(p.y < lb_coords[boardType][DIM_Y][y]) { break; }
        }
        keyByte=getBoardSymbol(x, y);

#ifdef DEBUG_LB
        sprintf(debugstr, "Board c: %d, %d - Byte %d\n", x, y, (int)keyByte );
        Serial.println(debugstr);
#endif 
        lcdBuffer[lcdBufPos][lcdX]=keyByte;
        lcd.write(keyByte);
        lcdX++;
#ifdef ENABLE_KEY
        Keyboard.print(keyByte);
#endif
    } else if (p.x < 30) {
        keyByte=0x0;
    }

    if(lcdX>15) {
        /* Scroll bottom line up and reset buffer pointer */
        lcd.clear();
        lcd.setCursor(0,0);
        lcdBuffer[lcdBufPos][16]='\0';
        lcd.print(lcdBuffer[lcdBufPos]);
        lcdX=0;
        lcdBufPos++;    // Incr the ring buffer pointer
        if(lcdBufPos>LBUF_SIZE) { lcdBufPos=0; }
        Serial.print(F("Incr ring pointer: "));
        Serial.println(lcdBufPos);
        lcd.setCursor(0,1);
    }
    delay(50);
}

/* ******************************************************* */

int adc_key_val[] = { 50, 200, 400, 600, 800 };
#define NUM_KEYS  5
#define BUTTON_PIN 0
int get_key() {
    unsigned int adcval = analogRead(BUTTON_PIN);
    int k;
    for (k=0; k< NUM_KEYS; k++) {
        if (adcval < adc_key_val[k]) {
            return k;
        }
    }
    if (k >= NUM_KEYS) {
        return -1;
    }
}

void setup() {
#ifdef ENABLE_KEY
    Keyboard.begin();
#endif
    Serial.begin(9600);
    /* Null-terminate the ring buffer lines */
    for(int x=0; x<=LBUF_SIZE; x++) {
        lcdBuffer[x][0]='\0';
    }
    // set up the LCD's number of columns and rows: 
    lcd.begin(16, 2);
    lcd.setCursor(0,0);
    lcd.print(F("e Letterboard"));    // Splash screen @ power on
    lcd.setCursor(0,1);
    lcd.print(F(SW_VERS));
    /* Restore settings from eeprom */
    boardType=EEPROM.read(EEPROM_BOARD_TYPE);
    letterCase=EEPROM.read(EEPROM_LETTER_CASE);
    delay(3500);
    lcd.clear();
    lcd.setCursor(0,1);
    // Read out first, bugus, coordinate from screen
    p = ts.getPoint();
    readCalibration(EEPROM_CAL_START);
    delay(50);    // And settle down
}

