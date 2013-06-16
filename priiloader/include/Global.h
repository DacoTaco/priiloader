/*

priiloader(preloader mod) - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2013-2013  DacoTaco

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
//global functions,defines & variables for priiloader

#ifndef _GLOBAL_H_
#define _GLOBAL_H_

//DEFINES
//---------------
//#define PATCHED_ES //needed if your libogc version is lower then libogc 1.8.3. 1.8.3 is needed for both the tmd views for ES_GetTMDView as the correct way to do ES_OpenTitleContent
#ifdef PATCHED_ES
#define ES_OpenTitleContent(x,y,z) ES_OpenTitleContent_patched(x,y,z)
#endif

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))

#ifdef DEBUG
	#define gdprintf gprintf
#else
	#define gdprintf(...)
#endif

//INCLUDES
//---------------
#ifdef PATCHED_ES
#include "es.h"
#else
#include <ogc/es.h>
#endif
#include <gctypes.h>
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <network.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include "gecko.h"

//STRUCTS
//--------------

#ifdef PATCHED_ES
//patch to libogc was submitted for the following struct and is included starting libogc 1.8.3
typedef struct _tmd_view_content_t
{
  uint32_t cid;
  uint16_t index;
  uint16_t type;
  uint64_t size;
} __attribute__((packed)) tmd_view_content_t;

typedef struct _tmd_view_t
{
	uint8_t version; // 0x0000;
	uint8_t filler[3];
	uint64_t sys_version; //0x0004
	uint64_t title_id; // 0x00c
	uint32_t title_type; //0x0014
	uint16_t group_id; //0x0018
	uint8_t reserved[0x3e]; //0x001a this is the same reserved 0x3e bytes from the tmd
	uint16_t title_version; //0x0058
	uint16_t num_contents; //0x005a
	tmd_view_content_t contents[]; //0x005c
}__attribute__((packed)) tmd_view;
#endif


//TYPEDEFS
//--------------

//VARIABLES
//--------------
extern GXRModeObj *rmode;
extern void *xfb;

//FUNCTIONS
//---------------
extern "C"
{
#ifdef PATCHED_ES
	extern s32 ES_OpenTitleContent_patched(u64 titleID, tikview *views, u16 index);
#endif
}

void InitVideo ( void );

#define free_pointer(x) if(x != NULL) { free(x);x=NULL; }
void Control_VI_Regs ( u8 mode );
s8 InitNetwork();
u8 GetUsbOnlyMode();
u8 ToggleUSBOnlyMode();
bool PollDevices( void );
void ShutdownDevices();
bool RemountDevices( void );
s8 GetMountedValue(void);

#endif