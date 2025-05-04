/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019  crediar & DacoTaco

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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <gccore.h>
#include <ogc/usbgecko.h>
#include <debug.h>

#include "gecko.h"
#include "mount.h"

static u8 GeckoFound = 0;
static u8 DumpDebug = 0;

bool IsUsbGeckoDetected()
{
	return GeckoFound;
}

void InitGDBDebug(void)
{
	DEBUG_Init(GDBSTUB_DEVICE_USB, 1);
	return;
}

void CheckForGecko(void)
{
	GeckoFound = usb_isgeckoalive(EXI_CHANNEL_1);
	if (GeckoFound)
		usb_flush(EXI_CHANNEL_1);
	return;
}

static void __write_str(const char* str, const u32 len)
{
	if(GeckoFound)
	{
		usb_sendbuffer( EXI_CHANNEL_1, str, len );
		usb_flush(EXI_CHANNEL_1);
	}
	
	if (DumpDebug > 0 && GetMountedFlags() > 0)
	{
		FILE* fd = fopen(BuildPath("/prii.log").c_str(), "ab");
		if(fd != NULL)
		{
			fwrite(str, 1, len, fd);
			fclose(fd);
		}
	}
}

void gprintf( const char *str, ... )
{
	if(!GeckoFound && (!DumpDebug || GetMountedFlags() <= 0))
		return;

	//lets first compile the input
	char* inputBuffer = NULL;
	char* outputBuffer = NULL;
	u32 bufferLength = 0x100;
	u32 length;
	va_list args;

	// Current date/time based on current system, converted to tm struct for local timezone
	time_t LeTime = time(0);
	struct tm* localtm = localtime(&LeTime);

	while(true)
	{
		char* tmp = (char*)realloc(inputBuffer, bufferLength);
		if(!tmp)
		{
			const char* err = "Failed to allocate input prefix\r\n";
			__write_str(err, strlen(err));
			return;
		}

		inputBuffer = tmp;
		length = snprintf(inputBuffer, bufferLength, "%02d:%02d:%02d : %s\r\n", localtm->tm_hour, localtm->tm_min, localtm->tm_sec, str);
		if(length > 0 && length >= bufferLength)
		{
			bufferLength = length+1;
			continue;
		}

		break;
	}

	if(length <= 0)
	{
		const char* err = "Failed to prepare input prefix\r\n";
		__write_str(err, strlen(err));
		return;
	}

	//string prefix has been created, now lets plug in the args
	while(true)
	{
		char* tmp = (char*)realloc(outputBuffer, bufferLength);
		if(!tmp)
		{
			const char* err = "Failed to allocate input prefix\r\n";
			__write_str(err, strlen(err));
			return;
		}

		outputBuffer = tmp;
		va_start(args, str);
		length = vsnprintf(outputBuffer, bufferLength, inputBuffer, args);
		va_end(args);
		if(length > 0 && length >= bufferLength)
		{
			bufferLength = length+1;
			continue;
		}
		
		break;
	}

	if(length <= 0)
	{
		const char* err = "Failed to prepare output prefix\r\n";
		__write_str(err, strlen(err));
		return;
	}
	
	__write_str(outputBuffer, length);
	free(inputBuffer);
	free(outputBuffer);
}

void SetDumpDebug( u8 value )
{
	DumpDebug = value > 0 ? 1 : 0;
	
	//don't dump or no devices mounted?
	if (!DumpDebug || GetMountedFlags() <= 0)
		return;
	
	//create file, or re-open and add lining
	FILE* fd = fopen(BuildPath("/prii.log").c_str(), "ab");
	if (fd != NULL)
	{
		char str[] = "--------gecko_output_enabled------\r\n\0";
		fwrite(str, 1, strlen(str), fd);
		fclose(fd);
	}
	else
	{
		//we failed. fuck this shit
		DumpDebug = 0;
	}

	return;
}