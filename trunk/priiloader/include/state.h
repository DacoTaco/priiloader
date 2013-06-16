/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2013  crediar

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
#ifndef _STATE_H_
#define _STATE_H_

#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <string.h>

#include "Global.h"

#define TYPE_RETURN 3
#define TYPE_NANDBOOT 4
#define TYPE_SHUTDOWNSYSTEM 5
#define TYPE_UNKNOWN 255
#define RETURN_TO_MENU 0
#define RETURN_TO_SETTINGS 1
#define RETURN_TO_ARGS 2

typedef struct {
	u32 checksum;
	u8 flags;
	u8 type;
	u8 discstate;
	u8 returnto;
	u32 unknown[6];
} __attribute__((packed)) StateFlags;

typedef struct {
       u32 checksum;
       u32 argsoff;
       u8 unk1;
       u8 unk2;
       u8 apptype;
       u8 titletype;
       u32 launchcode;
       u32 unknown[2];
       u64 launcher;
       u8 argbuf[0x1000];
} __attribute__((packed)) NANDBootInfo;

s32 CheckBootState( void );
s32 ClearState( void );
StateFlags GetStateFlags( void );
s32 SetBootState( u8 type , u8 flags , u8 returnto , u8 discstate );
s8 VerifyNandBootInfo ( void );
s32 SetNandBootInfo(void);
#endif
