/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2013 DacoTaco

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

#ifndef _TITLES_H_
#define _TITLES_H_

#include <gccore.h>
#include <wiiuse/wpad.h>

#include "Global.h"
#include "state.h"
#include "settings.h"
#include "playlog.h"
#include "font.h"

//defines
#define TITLE_UPPER(x) (u32)(x >> 32)
#define TITLE_LOWER(x) (u32)(x & 0xFFFFFFFF)

//structs & classes
//-------------------
typedef struct {
	u64 title_id;
	std::string name_ascii;
	u8 name_unicode[84];
	u32 content_id;
} title_info;

//copy pasta from wiibrew
typedef struct {
    u8 zeroes[128]; // padding
    u32 imet; // "IMET"
    u8 unk[8];  // 0x0000060000000003 fixed, unknown purpose
    u32 sizes[3]; // icon.bin, banner.bin, sound.bin
    u32 flag1; // unknown
    u8 names[10][84]; // Japanese, English, German, French, Spanish, Italian, Dutch, unknown, unknown, Korean
    u8 zeroes_2[588]; // padding
    u8 crypto[16]; // MD5 of 0x40 to 0x640 in header. crypto should be all 0's when calculating final MD5
} IMET;

//functions
//-------------
s8 CheckTitleOnSD(u64 id);
s8 GetTitleName(u64 id, u32 app, char* name,u8* _dst_uncode_name);
s32 LoadListTitles( void );

#endif