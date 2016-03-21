#include "lib.h"

void *memset(void * ptr, int value, unsigned int num){
    unsigned char *p = ptr;
    while (num) {
        *p++ = value;
        num--;
    }
    return ptr;
}

int strcomp(char* s1, char* s2, unsigned int size){
    for(int i = 0; i < size*2; i++){
        if(s1[i] != s2[i]) return 0;
    }
    return 1;
}

void strcopy(char* dest, char* source, unsigned int size){
	for(int i = 0; i < size*2; i++) dest[i] = source[i];
}

int memcmp(void* buf1, void* buf2, int size){
	int equal = 0;
	for(int i = 0; i < size; i++){
		if(*((unsigned char*)buf1 + i) != *((unsigned char*)buf2 + i)){
			equal = i;
			break;
		}
	}
	return equal;
}

int atoi(const char* nptr){
    int result   = 0,
        position = 1;
    const char* p = nptr;
    
    while(*p) ++p;

    for(--p; p >= nptr; p--){
        if(*p < 0x30 || *p > 0x39) break;
        else{
            result += (position) * (*p - 0x30);
            position *= 10;
        }
    }
    result = ((nptr[0] == '-')? -result : result);
    return result;
}

unsigned isPressed(unsigned bitfield){
	return ((~*(unsigned *)0x10146000) & 0xFFF) == (bitfield & 0xFFF) ? 1 : 0;
}