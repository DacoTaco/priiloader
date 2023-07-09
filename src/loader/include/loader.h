/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console
Executable Loader - Loads any executable who has been loaded into memory

Copyright (C) 2008-2019  DacoTaco

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

#ifndef _LOADER_H_
#define _LOADER_H_

enum {
	BINARY_TYPE_DEFAULT,
	BINARY_TYPE_SYSTEM_MENU,
	BINARY_TYPE_SYSTEM_MENU_VWII,
};

//NOTE : its a bit odd, but in the loader its parameters are defined by this define.
//if you want to change the parameters, do it here so it'll change everywhere else (typedef + implementation)
#define _LDR_PARAMETERS void* binary, void* parameter, u32 parameterCount, u8 binaryType
typedef void (*loader_t)(_LDR_PARAMETERS);

#endif