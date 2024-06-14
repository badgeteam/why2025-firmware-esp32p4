#pragma once

#define MAX_DATA_LEN 65536

#define SIDMODEL_8580 8580
#define SIDMODEL_6581 6581

// #define DEFAULT_SAMPLERATE 44100
#define DEFAULT_SIDMODEL SIDMODEL_6581

extern void libcsid_init(int samplerate, int sidmodel);

extern int libcsid_load(unsigned char *buffer, int bufferlen, int subtune);

extern char const *libcsid_getauthor();
extern char const *libcsid_getinfo();
extern char const *libcsid_gettitle();

extern void libcsid_render(unsigned short *output, int numsamples);
