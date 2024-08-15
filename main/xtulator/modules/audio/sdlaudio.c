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

/*
	NOTE: Maybe implement some kind of crude time-stretching?
*/

#include "../../config.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "sdlaudio.h"
#include "pcspeaker.h"
#include "opl2.h"
#include "blaster.h"
#include "../../machine.h"
#include "../../timing.h"
#include "../../utility.h"
#include "../../debuglog.h"
#include <sys/time.h>

int16_t sdlaudio_buffer[SAMPLE_BUFFER], sdlaudio_bufferpos = 0;
double sdlaudio_rateFast;

uint8_t sdlaudio_firstfill = 1, sdlaudio_timeIdx = 0;

uint32_t sdlaudio_timer;
uint64_t sdlaudio_cbTime[10] = { //history of time between callbacks to dynamically adjust our sample generation
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
double sdlaudio_genSampRate = SAMPLE_RATE;
double sdlaudio_genInterval;

volatile uint8_t sdlaudio_updateTiming = 0;

MACHINE_t* sdlaudio_useMachine = NULL;

void sdlaudio_moveBuffer(int16_t* dst, int len);

void sdlaudio_fill(void* udata, uint8_t* stream, int len) {
}

int sdlaudio_init(MACHINE_t* machine) {
	return 0;
}

void sdlaudio_bufferSample(int16_t val) {
}

void sdlaudio_updateSampleTiming() {
	if (sdlaudio_updateTiming == SDLAUDIO_TIMING_FAST) {
		timing_updateIntervalFreq(sdlaudio_timer, sdlaudio_rateFast);
	}
	else if (sdlaudio_updateTiming == SDLAUDIO_TIMING_NORMAL) {
		timing_updateIntervalFreq(sdlaudio_timer, SAMPLE_RATE);
	}
	sdlaudio_updateTiming = 0;
}

//I need to make this use a ring buffer soon...
void sdlaudio_moveBuffer(int16_t* dst, int len) {
}

void sdlaudio_generateSample(void* dummy) {
	int16_t val;

	val = pcspeaker_getSample(&sdlaudio_useMachine->pcspeaker) / 3;
	//val += opl2_generateSample(&sdlaudio_useMachine->OPL2) / 3;
	if (sdlaudio_useMachine->mixOPL) {
		int16_t OPLsample[2];
		//val += sdlaudio_getOPLsample() / 2;
		OPL3_GenerateStream(&sdlaudio_useMachine->OPL3, OPLsample, 1);
		val += OPLsample[0] / 2;
	}
	if (sdlaudio_useMachine->mixBlaster) {
		val += blaster_getSample(&sdlaudio_useMachine->blaster) / 3;
	}

	sdlaudio_bufferSample(val);
}
