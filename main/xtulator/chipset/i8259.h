/*
  XTulator: A portable, open-source 80186 PC emulator.
  Copyright (C)2020 Mike Chambers

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef _I8259_H_
#define _I8259_H_

#include <stdint.h>

typedef struct {
	uint8_t imr; //mask register
	uint8_t irr; //request register
	uint8_t isr; //service register
	uint8_t icwstep; //used during initialization to keep track of which ICW we're at
	uint8_t icw[5];
	uint8_t ocw[5];
	uint8_t intoffset; //interrupt vector offset
	uint8_t priority; //which IRQ has highest priority
	uint8_t autoeoi; //automatic EOI mode
	uint8_t readmode; //remember what to return on read register from OCW3
	uint8_t vector;
	uint8_t lastintr;
	uint8_t enabled;
} I8259_t;

void i8259_init(I8259_t* i8259);
void i8259_doirq(I8259_t* i8259, uint8_t irqnum);
uint8_t i8259_nextintr(I8259_t* i8259);
void i8259_write(I8259_t* i8259, uint16_t portnum, uint8_t value);
uint8_t i8259_read(I8259_t* i8259, uint16_t portnum);

#endif
