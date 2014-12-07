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

#define BUTTON_LEFT 3
#define BUTTON_RIGHT 0
#define BUTTON_UP 1
#define BUTTON_DOWN 2
#define BUTTON_SELECT 4

// EEPROM locations for saved settings values
#define EEPROM_BOARD_TYPE 0
#define EEPROM_LETTER_CASE 1
#define EEPROM_CAL_START 10

// TouchScreen Pins
#define XP A3
#define XM A2
#define YM A5
#define YP A4
#define MIN_TS_Z 1

#define ASCII_A 65
#define ASCII_0 48

#define BOARD_ABC 0
#define BOARD_QWERTY 1
#define BOARD_NUM 1
#define NUM_BOARDS 3

// Uncomment this to enable USB keyboard output
// Leave commented for testing with LCD only
//#define ENABLE_KEY

#define DEBUG_LB

/***********  Define letterboard properties  ********/
// LB_ALPHA is standard ABC alphabet
// Number of letters across
#define LB_ALPHA_X 6
// Number of letters down
#define LB_ALPHA_Y 5
// Same definitions for the numbers board
#define LB_NUM_X 5
#define LB_NUM_Y 3
char boardNumOperators[] = "/*-+=";
// Space  backspace  .  ?  Z
char alphaColumnSix[] = { 32, 8, '.', '?', 'Z' };

uint8_t board_dims[NUM_BOARDS][2] = {
    { LB_ALPHA_X, LB_ALPHA_Y },
    { LB_NUM_X,   LB_NUM_Y },
    {},
};

int16_t lb_coords[3][2][6];
#define DIM_X 0    // Index 0 for X in all arrays
#define DIM_Y 1    // Index 1 for Y
// Data for the coordinates is now stored in EEPROM

