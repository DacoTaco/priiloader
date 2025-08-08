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
#define INPUT_BUTTON_LEFT			0x00000001
#define INPUT_BUTTON_RIGHT			0x00000002
#define INPUT_BUTTON_DOWN			0x00000004
#define INPUT_BUTTON_UP				0x00000008
#define INPUT_TRIGGER_Z				0x00000010
#define INPUT_TRIGGER_R				0x00000020
#define INPUT_TRIGGER_L				0x00000040
#define INPUT_BUTTON_A				0x00000100
#define INPUT_BUTTON_B				0x00000200
#define INPUT_BUTTON_X				0x00000400
#define INPUT_BUTTON_Y				0x00000800
#define INPUT_BUTTON_START			0x00001000
#define INPUT_BUTTON_STM			0x10000000

#define RESET_UNPRESSED (((*(vu32*)0xCC003000)>>16)&1)

s8 Input_Init( void );
void Input_Shutdown( bool disconnectWPAD );
u32 Input_ScanPads( void );
u32 Input_ButtonsDown( bool _overrideSTM );
u32 Input_ButtonsDown( void );

#endif