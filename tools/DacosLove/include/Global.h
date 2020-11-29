/*

priiloader(preloader mod) - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2013-2019  DacoTaco

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

#define TEXT_OFFSET(X) ((((rmode->viWidth) / 2 ) - (sizeof((X))*13/2))>>1)

#ifdef DEBUG
	#define gdprintf gprintf
#else
	#define gdprintf(...)
#endif

//INCLUDES
//---------------
#include <ogc/es.h>
#include <gctypes.h>
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/param.h>
#include <errno.h>
#include <network.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include "gecko.h"

//STRUCTS
//--------------


//TYPEDEFS
//--------------

//VARIABLES
//--------------
extern GXRModeObj *rmode;
extern void *xfb;

//FUNCTIONS
//---------------
void InitVideo ( void );
s8 InitNetwork();
u8 GetUsbOnlyMode();
u8 ToggleUSBOnlyMode();
bool PollDevices( void );
void ShutdownDevices();
bool RemountDevices( void );
s8 GetMountedValue(void);

#endif