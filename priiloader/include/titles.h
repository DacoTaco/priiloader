/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019 DacoTaco

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
#define TITLE_TYPE_INVALID              0x00000000
#define TITLE_TYPE_ESSENTIAL            0x00000001
#define TITLE_TYPE_DISC                 0x00010000
#define TITLE_TYPE_DOWNLOAD             0x00010001
#define TITLE_TYPE_SYSTEM               0x00010002
#define TITLE_TYPE_GAMECHANNEL          0x00010004
#define TITLE_TYPE_DLC                  0x00010005
#define TITLE_TYPE_HIDDEN               0x00010008

#define TITLE_COMBINE(upper, lower)     (u64)(((u64)upper << 32) + (lower & 0xFFFFFFFF))
#define TITLE_UPPER(x)                  (u32)(x >> 32)
#define TITLE_LOWER(x)                  (u32)(x & 0xFFFFFFFF)
#define TITLE_GAMEID_TYPE(x)            (u8)((x >> 24) & 0x000000FF)

#define TITLE_NTSC                      0
#define TITLE_NTSC_J                    1
#define TITLE_PAL                       2

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

//The known HBC titles
extern const title_info HBC_Titles[];
extern const s32 HBC_Titles_Size;

//functions
//-------------
s8 CheckTitleOnSD(u64 id);
s8 GetTitleName(u64 id, u32 app, char* name,u8* _dst_uncode_name);
u8 GetTitleRegion(u32 lowerTitleId);
s8 SetVideoModeForTitle(u32 lowerTitleId);
s32 LoadListTitles( void );

#endif