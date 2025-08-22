/*
vWii.cpp - vWii compatibility utils for priiloader

Copyright (C) 2022
GaryOderNichts
DacoTaco

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

#include <cstring>

#include "vWii.h"
#include "gecko.h"
#include <ogc/isfs.h>
#include <ogc/ipc.h>

#define LT_CHIPREVID (*(reinterpret_cast<vu32*>(0xcd8005a0)))

static WiiUConfig wiiuConfig;
static WiiUArgs wiiuArgs;

bool CheckvWii(void)
{
	// Check LT_CHIPREVID for cafe magic
	return (LT_CHIPREVID & 0xffff0000) == 0xcafe0000;
}

void ImportWiiUConfig(void)
{
	if (GetWiiUConfig() == nullptr)
	{
		s32 confFd = ISFS_Open("/shared2/sys/compat/conf.bin", ISFS_OPEN_READ);
		if (confFd >= 0)
		{
			alignas(0x20) u8 buf[0x500];
			if (ISFS_Read(confFd, buf, sizeof(buf)) == sizeof(WiiUConfig))
				memcpy(&wiiuConfig, buf, sizeof(wiiuConfig));

			ISFS_Close(confFd);
		}
	}

	if (GetWiiUArgs() == nullptr)
	{
		s32 argsFd = ISFS_Open("/shared2/sys/compat/args.bin", ISFS_OPEN_READ);
		if (argsFd >= 0)
		{
			alignas(0x20) u8 buf[0x80];
			if (ISFS_Read(argsFd, buf, sizeof(buf)) == sizeof(WiiUArgs))
				memcpy(&wiiuArgs, buf, sizeof(wiiuArgs));

			ISFS_Close(argsFd);
		}
	}
}

const WiiUConfig* GetWiiUConfig(void)
{
	if (wiiuConfig.magic != WIIU_MAGIC)
		return nullptr;

	return &wiiuConfig;
}

const WiiUArgs* GetWiiUArgs(void)
{
	if (wiiuArgs.magic != WIIU_MAGIC)
		return nullptr;

	return &wiiuArgs;
}

void SetWiiUArgs(const WiiUArgs* args)
{
	// Create the file in case it doesn't exist yet
	ISFS_CreateFile("/shared2/sys/compat/args.bin", 0, 3, 3, 3);

	// SM checks owner and group
	ISFS_SetAttr("/shared2/sys/compat/args.bin", 0x1000, 1, 0, 3, 3, 3);

	s32 argsFd = ISFS_Open("/shared2/sys/compat/args.bin", ISFS_OPEN_WRITE);
	if (argsFd >= 0)
	{
		alignas(0x20) u8 buf[0x80];
		memcpy(buf, args, sizeof(*args));
		ISFS_Write(argsFd, buf, sizeof(*args));

		ISFS_Close(argsFd);
	}
}

static s32 __stm_async_callback(s32 result, void *usrdata)
{
	return 0;
}

s32 STM_DMCUWrite(u32 message)
{
	s32 stm_fd = IOS_Open("/dev/stm/immediate", 0);
	if (stm_fd < 0)
		return stm_fd;

	alignas(0x20) u32 stm_inbuf[8];
	alignas(0x20) u32 stm_outbuf[8];
	stm_inbuf[0] = message;
	s32 ret = IOS_IoctlAsync(stm_fd, 0x8001, stm_inbuf, 0x20, stm_outbuf, 0x20, __stm_async_callback, NULL);
	
	IOS_Close(stm_fd);

	return ret;
}

