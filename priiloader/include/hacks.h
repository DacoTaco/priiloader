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

#ifndef _HACKS_H_
#define _HACKS_H_

#include "Global.h"
#include "mount.h"
#include <string>

struct system_patch {
	std::vector<uint8_t> hash;
	u32 offset = 0;
	std::vector<uint8_t> patch;
}ATTRIBUTE_ALIGN(32);

struct system_hack {
	std::string desc;
	std::string masterID;
	std::string requiredID;
	u16 max_version = 0;
	u16 min_version = 0;
	std::vector< system_patch > patches;
}ATTRIBUTE_ALIGN(32);

extern std::vector<system_hack> system_hacks;
extern std::vector<u8> states_hash;

s8 LoadSystemHacks( StorageDevice source );
s32 GetMasterHackIndexByID(const std::string& ID );

#endif
