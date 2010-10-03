/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2009  crediar

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
#include "Global.h"
#include <string>

struct hack {
	char *desc;
	unsigned int version;
	std::vector<unsigned int> offset;
	std::vector<unsigned int> value;
};
struct patch_struct {
	std::vector<unsigned int> hash;
	std::vector<unsigned int> patch;
};
struct hack_hash {
	std::string desc;
	unsigned int max_version;
	unsigned int min_version;
	unsigned int amount;
	std::vector< patch_struct > patches;
};
/*
struct hack_hash {
	std::string desc;
	unsigned int max_version;
	unsigned int min_version;
	unsigned int amount;
	std::vector< std::vector<unsigned int> > hash;
	std::vector< std::vector<unsigned int> > patch;
};
*/
u32 LoadHacks( bool Force_Load_Nand );
s8 LoadHacks_Hash( bool Force_Load_Nand );
