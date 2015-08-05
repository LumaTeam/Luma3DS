#ifndef LIB_H
#define LIB_H

#define BUTTON_A      (1 << 0)
#define BUTTON_B      (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START  (1 << 3)
#define BUTTON_RIGHT  (1 << 4)
#define BUTTON_LEFT   (1 << 5)
#define BUTTON_UP     (1 << 6)
#define BUTTON_DOWN   (1 << 7)
#define BUTTON_R1     (1 << 8)
#define BUTTON_L1     (1 << 9)
#define BUTTON_X      (1 << 10)
#define BUTTON_Y      (1 << 11)
#define BUTTON_ZL     (1 << 14)
#define BUTTON_ZR     (1 << 15)

void* memset(void * ptr, int value, unsigned int num);
int strcomp(char* s1, char*s2, unsigned int size);
void strcopy(char* dest, char* source, unsigned int size);
int memcmp(void* buf1, void* buf2, int size);
unsigned isPressed(unsigned bitfield);

#endif