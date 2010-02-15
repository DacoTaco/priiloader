// OpenDolBoot.h : Defines the entry point for the console application.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#define VERSION "0.1"
#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#define SwapEndian(x) _byteswap_ulong(x)
#else
#define SwapEndian(x) __builtin_bswap32(x)
#include <unistd.h>
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

typedef struct{
	int padding[4];
	int code_part1[100];
	int padding2[155];
	unsigned int entrypoint_dol;
	int code_part2[64];
} Nandcode;

void Display_Parameters ( void );
