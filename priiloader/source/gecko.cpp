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

void gprintf( const char *str, ... )
{
	if(!GeckoFound && (!DumpDebug || GetMountedFlags() <= 0))
		return;

	char astr[2048];
	s32 size = 0;
	memset(astr, 0, sizeof(astr));

	// Current date/time based on current system, converted to tm struct for local timezone
	time_t LeTime = time(0);
	struct tm* localtm = localtime(&LeTime);

	char nstr[2048];
	memset(nstr, 0, 2048);
	snprintf(nstr, 2048, "%02d:%02d:%02d : %s\r\n", localtm->tm_hour, localtm->tm_min, localtm->tm_sec, str);

	va_list ap;
	va_start(ap, str);
	size = vsnprintf(astr, 2047, nstr, ap);
	va_end(ap);
	astr[size] = '\0';

	if(GeckoFound)
	{
		usb_sendbuffer( 1, astr, size );
		usb_flush(EXI_CHANNEL_1);
	}

	if (DumpDebug > 0 && GetMountedFlags() > 0)
	{
		FILE* fd = fopen(BuildPath("/prii.log").c_str(), "ab");
		if(fd != NULL)
		{
			fwrite(astr,1,size,fd);
			fclose(fd);
		}
	}
	return;
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