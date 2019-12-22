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

//NOTE : if we change the _boot parameters, we need to change the loader & set loader address parameters as well
void _boot(void* binary, void* parameter, u32 parameterCount, u8 isSystemMenu); 
void(*loader)(void* addr, void* parameter, u32 parameterCount, u8 isSystemMenu);
#define SET_LOADER_ADDRESS(x) loader = (void (*)(void*,void*,u32,u8))(x)


#endif