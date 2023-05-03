/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019  crediar

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

//boot states
#define TYPE_WC24				0x00
#define TYPE_EJECTDISC			0x01
#define TYPE_REBOOT				0x02
#define TYPE_RETURN				0x03
#define TYPE_NANDBOOT			0x04
#define TYPE_SHUTDOWNSYSTEM		0x05
#define TYPE_UNKNOWN			0xFF

//return to flags
#define RETURN_TO_MENU			0x00
#define RETURN_TO_SETTINGS		0x01
#define RETURN_TO_ARGS			0x02

//disc state
#define DISCSTATE_WII			0x01
#define DISCSTATE_GC			0x02
#define DISCSTATE_OPEN			0x03

//additional flags
#define STATE_FLAG0				0x00
#define STATE_FLAG1				0x80
#define STATE_FLAG2				0x40
#define STATE_FLAG3				0x04
#define STATE_FLAG4				0x02
#define STATE_FLAG5				0x01

#define FLAGS_STARTWIIGAME		(STATE_FLAG1 | STATE_FLAG2 | STATE_FLAG5)
#define FLAGS_STARTGCGAME		(STATE_FLAG1 | STATE_FLAG4)

typedef struct {
	u32 checksum;
	u8 flags;
	u8 type;
	u8 discstate;
	u8 returnto;
	u32 unknown[6];
} __attribute__((packed, aligned(4))) StateFlags;

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
} __attribute__((packed, aligned(4))) NANDBootInfo;

typedef struct wii_state {
	s8 Shutdown:2;
	s8 Init:2;
	s8 ReloadedIOS:2;
	s8 InMainMenu:2;
} wii_state;


extern wii_state system_state;
s32 CheckBootState( void );
s32 ClearState( void );
s32 GetStateFlags( StateFlags* state );
s32 SetBootState( u8 type , u8 flags , u8 returnto , u8 discstate );
s8 VerifyNandBootInfo ( void );
s32 SetNandBootInfo(void);
#endif
