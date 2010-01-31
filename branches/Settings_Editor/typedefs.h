#ifndef TYPEDEFS_H
#define TYPEDEFS_H

typedef unsigned char       u8;	//UCHAR_MAX		FF
typedef unsigned short     u16;	//USHRT_MAX		FF FF
typedef unsigned int       u32;	//UINT_MAX		FF FF FF FF
typedef unsigned long     ul32;	//ULONG_MAX		FF FF FF FF UL
typedef unsigned long long u64;	//ULLONG_MAX	FF FF FF FF FF FF FF FF

typedef char                n8;	//CHAR_MAX		FF    | 7F
typedef int                n16; //				FF FF | 7F FF

typedef signed char         s8;	//SCHAR_MAX		7F
typedef signed short       s16;	//SHRT_MAX		7F FF
typedef signed int         s32;	//INT_MAX		7F FF FF FF
typedef signed long       sl32;	//LONG_MAX		7F FF FF FF L
typedef signed long long   s64;	//LLONG_MAX		7F FF FF FF FF FF FF FF

#define PORT_START 27592
#define MAX_LISTENERS 20
#define BUF_SIZE 40960
#define NO_AUTH
/* DESTIP is the server ip. note to self to add a resolving later on to resolve schtserv.ath.cx. 
!!!!! but only when a sertain parameter is given !!!!!
cause jp client will resolve gsproduc.ath.cx as loopback ip 127.0.0.1*/
#define DESTIP { 208, 69, 57, 87 }
//#define DESTIP { 198, 176, 8, 71 } //Sega
#define DESTCRCSIZE 16
#define CRCKEY { 0xAD, 0x0F, 0x0C, 0x21, 0x22, 0x54, 0x63, 0xCB }
#define DEBUGMODE //Additional output

#endif
