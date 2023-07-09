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
#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ogc/ipc.h>
#include "mem2_manager.h"
#include "dvd.h"
#include "gecko.h"

typedef struct _di_iovector
{
	ioctlv ioctlv[8];
} di_iovector;

u32 _di_lasterror[0x08] [[gnu::aligned(32)]];

s32 di_fd = -1;
s8 async_called = 0;
ioctlv iovectors[8] [[gnu::aligned(64)]] = { 0 };
u32 cmd[8] [[gnu::aligned(64)]] = { 0 };


//see https://wiibrew.org/wiki/Hardware/Drive_Interface for the address
//see https://www.gc-forever.com/yagcd/chap5.html#sec5.7 for registries
const vu32* __diReg = (u32*)0xCD806000;

static s32 _driveStopped(s32 result, void* usrdata)
{
	async_called = 0;
	return 1;
}

s32 DVDInit(void)
{
	if (di_fd >= 0)
		return 1;

	di_fd = IOS_Open("/dev/di", 0);

	return di_fd < 0 ? di_fd : 1;
}

inline s32 DVDAsyncBusy(void)
{
	return async_called;
}

void DVDCloseHandle(void)
{
	if (di_fd < 0)
		return;

	IOS_Close(di_fd);
	di_fd = -1;
}

s32 DVDDiscAvailable(void)
{
	return !(__diReg[1] & 1);
}

s32 DVDStopDrive(void)
{
	//disabled as it said it was unavailable after some wii disc boot failure?
	/*if (DVDDiscAvailable() == 0)
		return 1;*/
		
	u32 value = 0;
	return DVDExecuteCommand(DVD_CMD_STOP_DRIVE, false, &value, 4, NULL, 0, NULL);
};

s32 DVDStopDriveAsync(void)
{	
	u32 value = 0;
	return DVDExecuteCommand(DVD_CMD_STOP_DRIVE, true, &value, 4, NULL, 0, _driveStopped);
};

s32 DVDResetDrive(void)
{
	return DVDResetDriveWithMode(DVD_RESETSOFT);
};
s32 DVDResetDriveWithMode(u32 mode)
{
	return DVDExecuteCommand(DVD_CMD_RESET_DRIVE, false, &mode, 4, NULL, 0, NULL);
}

s32 DVDReadGameID(void* dst, u32 len)
{
	//for some odd reason returns 0 ? :/
	/*if (DVDDiscAvailable() == 0)
		return -1;*/

	//its not that the output has to be 32 byte aligned, it'll raise an error anyway
	if (((u32)dst % 32) != 0)
		return -2;
	
	s32 ret = DVDExecuteCommand(DVD_CMD_READ_ID, false, NULL, 0, dst, len, NULL);
	if (ret < 0)
		return ret;

	return (ret == 1) ? ret : -ret;
}
s32 DVDAudioBufferConfig(u8 enable, s8 buffer_size)
{
	if (enable == 0 && buffer_size > 0)
		return -1;

	if (enable > 0 && buffer_size == 0)
		buffer_size = 10;

	u32 data[2] = {
		(u32)((enable > 0) & 1),
		(u32)(buffer_size & 0x0F)
	};

	s32 ret = DVDExecuteCommand(DVD_CMD_CONFIG_AUDIO_BUFFER, false, data, 8, NULL, 0, NULL);
	if (ret < 0)
		return ret;

	return (ret == 1) ? ret : -ret;
}

s32 DVDUnencryptedRead(u32 offset, void* buf, u32 len)
{
	//for some odd reason returns 0 ? :/
	/*if (DVDDiscAvailable() == 0)
		return -1;*/

	if (((u32)buf % 32) != 0)
		return -2;
		
	u32 data[2] = { 
		len, 
		offset >> 2
	}; 

	s32 ret = DVDExecuteCommand(DVD_CMD_UNENCRYPTED_READ, false, data , sizeof(data), buf, len, NULL);
	if (ret < 0)
		return ret;

	return (ret == 1) ? ret : -ret;
}

s32 DVDClosePartition()
{
	return DVDExecuteCommand(DVD_CMD_CLOSE_PARTITION, false, NULL, 0, NULL, 0, NULL);
}

s32 DVDOpenPartition(u32 offset, void* eticket, void* shared_cert_in, u32 shared_cert_in_len, void* tmd_out)
{
	if (shared_cert_in != NULL && shared_cert_in_len > 0)
		return -1;

	ioctlv data[4] [[gnu::aligned(64)]] = { 0 };

	data[0].data = eticket;
	data[0].len = (eticket == NULL) ? 0 : 0x02A4;
	data[1].data = shared_cert_in;
	data[1].len = (shared_cert_in == NULL) ? 0 : shared_cert_in_len;
	data[2].data = tmd_out;
	data[2].len = (tmd_out == NULL) ? 0 : 0x49E4;
	data[3].data = &_di_lasterror;
	data[3].len = 0x20;

	s32 ret = DVDExecuteVCommand(DVD_CMD_OPEN_PARTITION, false, 3, 2, &offset, 4, &data, sizeof(data), NULL, NULL);
	if (ret < 0)
		return ret;

	return (ret == 1) ? ret : -ret;
}

s32 DVDRead(off_t offset, void* output, u32 len)
{
	if (output == NULL || len == 0)
		return -1;

	//its not that the output has to be 32 byte aligned, but it performs better that way
	if (((u32)output % 32) != 0)
		return -2;

	u32 data[2] = { 
		len, 
		offset >> 2
	};
	
	s32 ret = DVDExecuteCommand(DVD_CMD_READ, false, data, sizeof(data), output, len, NULL);
	if (ret < 0)
		return ret;

	return (ret == 1) ? ret : -ret;
}

s32 DVDInquiry()
{
	u8 dummy[0x20] [[gnu::aligned(64)]]; 
	return DVDExecuteCommand(DVD_CMD_IDENTIFY, false, NULL, 0, dummy, 0x20, NULL);
}

s32 DVDExecuteVCommand(s32 command, bool do_async, s32 cnt_in, s32 cnt_io, void* cmd_input, u32 cmd_input_size, void* input, u32 input_size, ipccallback callback, void* userdata)
{
	if (DVDAsyncBusy()) // async was called or the dvd drive has no disk/not connected. lets not do this then since we dont support multiple asyncs...yet :P
		return -1;

	if (di_fd < 0)
		DVDInit();

	if (!di_fd)
		return -2;

	if (cmd_input_size > 0x1C) //di command is 0x20, -4 for the command = 0x1C
		return -3;

	memset(iovectors, 0, sizeof(iovectors));
	memset(cmd, 0, sizeof(cmd));

	cmd[0] = (u32)(command << 24);
	if (cmd_input != NULL && cmd_input_size > 0)
		memcpy(&cmd[1], cmd_input, cmd_input_size);

	iovectors[0].data = cmd;
	iovectors[0].len = sizeof(cmd);
	if (input != NULL && input_size > 0)
		memcpy(&iovectors[1], input, input_size);

	DCFlushRange(cmd, sizeof(cmd));
	DCFlushRange(iovectors, sizeof(iovectors));

	if (!do_async)
	{
		s32 ret = IOS_Ioctlv(di_fd, command, cnt_in, cnt_io, iovectors);
		return ret;
	}
	else
	{
		async_called = 1;
		IOS_IoctlvAsync(di_fd, command, cnt_in, cnt_io, iovectors, callback, userdata);
	}

	return 1;
}

s32 DVDExecuteCommand(u32 command, u8 do_async, void* input, s32 input_size, void* output, s32 output_size, ipccallback callback)
{
	if (DVDAsyncBusy()) // async was called or the dvd drive has no disk/not connected. lets not do this then since we dont support multiple asyncs...yet :P
		return -1;

	if (input_size > 0x1C) //di command is 0x20, -4 for the command = 0x1C
		return -2;

	if (di_fd < 0)
		DVDInit();

	if (!di_fd)
		return -3;

	if (output_size > 0 && output == NULL)
	{
		//memory failure :/
		return 0;
	}

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = (u32)(command << 24);
	if (input != NULL && input_size > 0)
		memcpy(&cmd[1], input, input_size);
	DCFlushRange(cmd, sizeof(cmd));

	if (!do_async)
	{
		s32 ret = IOS_Ioctl(di_fd, command, cmd, sizeof(cmd), output, output_size);
		return ret;
	}
	else
	{
		async_called = 1;
		IOS_IoctlAsync(di_fd, command, cmd, sizeof(cmd), output, output_size, callback, NULL);
	}

	return 1;
}
