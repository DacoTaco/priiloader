/*
OpenDolBoot - An utiltiy to make a dol binary into a title bootable binary on the Wii/vWii

Copyright (c) 2020 DacoTaco

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#pragma once

#ifdef WIN32
#include <winsock.h>
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#define SwapEndian(x) _byteswap_ulong(x)
#else
#include <arpa/inet.h>
#define SwapEndian(x) __builtin_bswap32(x)
#endif
#define IsBigEndian() (htonl(47) == 47)
#define ForceBigEndian(x) (IsBigEndian() ? x : SwapEndian(x))
#define BigEndianToHost(x) (!IsBigEndian() ? SwapEndian(x) : x)

#define MAX_TEXT_SECTIONS	7
#define MAX_DATA_SECTIONS	11

typedef struct {
	unsigned int offsetText[MAX_TEXT_SECTIONS];
	unsigned int offsetData[MAX_DATA_SECTIONS];
	unsigned int addressText[MAX_TEXT_SECTIONS];
	unsigned int addressData[MAX_DATA_SECTIONS];
	unsigned int sizeText[MAX_TEXT_SECTIONS];
	unsigned int sizeData[MAX_DATA_SECTIONS];
	unsigned int addressBSS;
	unsigned int sizeBSS;
	unsigned int entrypoint;
	int padding1[7];
} dolHeader;

static_assert(sizeof(dolHeader) == 0x100);