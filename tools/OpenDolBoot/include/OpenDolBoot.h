// OpenDolBoot.h : Defines the entry point for the console application.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#define VERSION "0.2"
#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#define SwapEndian(x) _byteswap_ulong(x)
#define SwapEndian16(x) _byteswap_ushort(x)
#else
#define SwapEndian(x) __builtin_bswap32(x)
#define SwapEndian16(x) __builtin_bswap16(x)
#include <unistd.h>
#include <string>
#endif

typedef struct {
	unsigned int offsetText[7];
	unsigned int offsetData[11];
	unsigned int addressText[7];
	unsigned int addressData[11];
	unsigned int sizeText[7];
	unsigned int sizeData[11];
	unsigned int addressBSS;
	unsigned int sizeBSS;
	unsigned int entrypoint;
	int padding1[7];
} dolhdr;

typedef struct {
	std::string filename;
	unsigned int file_size;
	unsigned char* data;
} file_info;

typedef struct{
	unsigned int startup;
	unsigned short upper_entrypoint;
	unsigned short lower_entrypoint;
	unsigned char nandcode[];
} Nandcode;

void Display_Parameters ( void );
