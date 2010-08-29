/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2009  crediar & DacoTaco

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

#include "gecko.h"
u8 GeckoFound = 0;
u8 ShowDebug = 0;

void CheckForGecko( void )
{
	GeckoFound = usb_isgeckoalive( EXI_CHANNEL_1 );
	if(GeckoFound)
		usb_flush(EXI_CHANNEL_1);
	return;
}
void gprintf( const char *str, ... )
{
	if(!GeckoFound && !ShowDebug)
		return;

	char astr[4096];

	va_list ap;
	va_start(ap,str);
	vsprintf( astr, str, ap );
	va_end(ap);

	if(GeckoFound)
	{
		usb_sendbuffer( 1, astr, strlen(astr) );
		usb_flush(EXI_CHANNEL_1);
	}
	if (ShowDebug)
	{
		printf(astr);
		time_t start,end;
		time(&start);
		//i hate while loops. but its safer when gprintf is called from a thread like the STM callback...
		//sleep seems to shit brix when called from the STM callback
		while(difftime(end, start) < 1)
		{
			time(&end);
		}
	}
}
void SetShowDebug( u8 value )
{
	if (value != 1 && value != 0)
		return;
	ShowDebug = value;
}
void InitGDBDebug( void )
{
	DEBUG_Init(GDBSTUB_DEVICE_USB, 1);
	return;
}
