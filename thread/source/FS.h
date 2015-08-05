#ifndef FS_H
#define FS_H
#include <stdio.h>
extern unsigned int fopen9(void *handle, wchar_t* name, unsigned int flag);
extern void fwrite9(void* handle, unsigned int* bytesWritten, void* dst, unsigned int size);
extern void fread9(void* handle, unsigned int* bytesRead, void *src, unsigned int size);
extern void fclose9(void *handle);

#endif