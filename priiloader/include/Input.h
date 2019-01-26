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

#ifndef _INPUT_H_
#define _INPUT_H_

//these are a copy of the GC PAD values. just named differently to distinguish them.
#define INPUT_BUTTON_LEFT			0x0001
#define INPUT_BUTTON_RIGHT			0x0002
#define INPUT_BUTTON_DOWN			0x0004
#define INPUT_BUTTON_UP				0x0008
#define INPUT_TRIGGER_Z				0x0010
#define INPUT_TRIGGER_R				0x0020
#define INPUT_TRIGGER_L				0x0040
#define INPUT_BUTTON_A				0x0100
#define INPUT_BUTTON_B				0x0200
#define INPUT_BUTTON_X				0x0400
#define INPUT_BUTTON_Y				0x0800
#define INPUT_BUTTON_MENU			0x1000
#define INPUT_BUTTON_START			0x1000

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

s8 Input_Init( void );
void Input_Shutdown( void );
u32 Input_ScanPads( void );
u32 Input_ButtonsDown( void );

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif