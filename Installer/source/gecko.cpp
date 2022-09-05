/*
Preloader/Priiloader Installer - An installation utiltiy for preloader (c) 2008-2020 crediar

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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <gccore.h>
#include <ogc/usbgecko.h>
#include <debug.h>

#include "gecko.h"

static u8 GeckoFound = 0;
bool IsUsbGeckoDetected() 
{
	return GeckoFound;
}
void InitGDBDebug(void)
{
	DEBUG_Init(GDBSTUB_DEVICE_USB, 1);
	return;
}

void CheckForGecko( void )
{
	GeckoFound = usb_isgeckoalive( EXI_CHANNEL_1 );
	if(GeckoFound)
		usb_flush(EXI_CHANNEL_1);
	return;
}

//unused in installer
void SetDumpDebug(u8 value)
{
	return;
}

void gprintf( const char *str, ... )
{
	if(!GeckoFound)
		return;

	char astr[2048];
	s32 size = 0;
	memset(astr,0,sizeof(astr));

	// Current date/time based on current system, converted to tm struct for local timezone
	time_t LeTime = time(0);
	struct tm* localtm = localtime(&LeTime);

	char nstr[2048];
	memset(nstr,0,2048);
	snprintf(nstr,2048, "%02d:%02d:%02d : %s\r\n",localtm->tm_hour,localtm->tm_min,localtm->tm_sec, str);

	va_list ap;
	va_start(ap,str);
	size = vsnprintf( astr, 2047, nstr, ap );
	va_end(ap);
	astr[size] = '\0';

	usb_sendbuffer(1, astr, size);
	usb_flush(EXI_CHANNEL_1);
	return;
}