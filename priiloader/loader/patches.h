/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console
patches - all info shared between loader & application on where to patch memory

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

#ifndef _PATCHES_H_
#define _PATCHES_H_

typedef struct {
	u32 offset;
	u32 patch_size;
	u8 patch[];
} offset_patch;

#endif