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
#include <vector>
#include <memory>
#include <string>

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

#define MAX_TITLE_NAME					84
#define IMET_HEADER_ID					0x494d4554

#define SYSTEMMENU_TITLEID				((u64)0x0000000100000002LL)

//structs & classes
//-------------------
enum TitleRegion {
	NTSC = 0,
	NTSC_J = 1,
	PAL = 2
};

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

typedef struct {
	std::string Name;
	unsigned char UnicodeName[MAX_TITLE_NAME];
} TitleName;

class TitleDescription
{
public:
	TitleDescription(u64 titleId, std::string name) : TitleId(titleId), Name(name){};
	u64 TitleId;
	std::string Name;
};

class TitleInformation 
{	
private:
	signed_blob* _titleTMD = NULL;
	u64 _titleId = 0;
	TitleName _titleName = { .Name = "" };
public:
	explicit TitleInformation(u64 titleId);
	explicit TitleInformation(u64 titleId, std::string name);
	~TitleInformation();
	u64 GetTitleId();
	std::string GetTitleIdString();
	bool IsInstalled();
	bool IsMovedToSD();
	tmd* GetTMD();
	signed_blob* GetRawTMD();
	TitleRegion GetTitleRegion();
	TitleName GetTitleName();
	void LaunchTitle();
};

//The known HBC titles
extern const std::vector<std::shared_ptr<TitleDescription>> HBCTitles;

//functions
//-------------
void SetVideoInterfaceConfig(std::shared_ptr<TitleInformation> title);
s8 SetVideoModeForTitle(std::shared_ptr<TitleInformation> title);
s32 LoadListTitles( void );

#endif