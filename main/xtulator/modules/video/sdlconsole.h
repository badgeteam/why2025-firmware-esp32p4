#ifndef _SDLCONSOLE_H_
#define _SDLCONSOLE_H_

#define SDLCONSOLE_EVENT_NONE		0
#define SDLCONSOLE_EVENT_KEY		1
#define SDLCONSOLE_EVENT_QUIT		2
#define SDLCONSOLE_EVENT_DEBUG_1	3
#define SDLCONSOLE_EVENT_DEBUG_2	4

int sdlconsole_init(char *title);
void sdlconsole_blit(uint32_t* pixels, int w, int h, int stride);
int sdlconsole_loop();
uint8_t sdlconsole_getScancode();
int sdlconsole_setWindow(int w, int h);
void sdlconsole_setTitle(char* title);

#endif
