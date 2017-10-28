/*

HTTP Parser by DacoTaco
Copyright (C) 2013-2017  DacoTaco

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


*/
//Note by DacoTaco : yes this isn't the prettiest HTTP Parser alive but it does the job better then loads of http parsers so stop complaining :)

#ifndef _HTTP_PARSER_H_
#define _HTTP_PARSER_H_

//includes
//---------------
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <network.h>
#include <unistd.h>
#include <string.h>
#include "gecko.h"
#include "Global.h"
#include "mem2_manager.h"

//defines
//----------------


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
s32 GetHTTPFile(const char *host,const char *file,u8*& Data, int* external_socket_to_use);
s32 ConnectSocket(const char *hostname, u32 port);
const char* Get_Last_reply( void );

#endif
