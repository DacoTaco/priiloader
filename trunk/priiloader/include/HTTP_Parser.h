//HTTP Parser By DacoTaco
//Note by DacoTaco : yes this isn't the prettiest HTTP Parser alive but it does the job so stop complaining :)

#ifndef _HTTP_PARSER_H_
#define _HTTP_PARSER_H_

//includes
//---------------
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include <wiiuse/wpad.h>
#include <network.h>
#include <unistd.h>
#include <string.h>
#include "gecko.h"
#include "Global.h"

//defines
//----------------
#define URL_REQUEST "GET /daco/version.dat HTTP/1.0\r\nHost: www.nyleveia.com\r\n\r\n\0"


//typedef
//---------------
typedef struct {
	u32 version;
	unsigned SHA1_Hash[5];
	u32 beta_version;
	u32 beta_number;
	unsigned beta_SHA1_Hash[5];
} UpdateStruct;


//functions
//--------------
s32 GetHTTPFile(const char *host,const char *file,u8*& Data, int external_socket_to_use);
s32 ConnectSocket(const char* hostname);
const char* Get_Last_reply( void );

#endif
