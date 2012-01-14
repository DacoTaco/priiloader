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
#include "Global.h" // i hate to do this, but i need to if i want to prevent a crash when dumping gecko output & switching device. also, why the fuck does fopen succeed if not device is mounted?!
u8 GeckoFound = 0;
u8 DumpDebug = 0;

void CheckForGecko( void )
{
	GeckoFound = usb_isgeckoalive( EXI_CHANNEL_1 );
	if(GeckoFound)
		usb_flush(EXI_CHANNEL_1);
	return;
}
void gprintf( const char *str, ... )
{
	if(!GeckoFound && !DumpDebug)
		return;

	char astr[2048];
	memset(astr,0,sizeof(astr));

	va_list ap;
	va_start(ap,str);
	vsprintf( astr, str, ap );
	va_end(ap);

	if(GeckoFound)
	{
		usb_sendbuffer( 1, astr, strlen(astr) );
		usb_flush(EXI_CHANNEL_1);
	}
	if (DumpDebug > 0 && GetMountedValue() > 0)
	{
		FILE* fd = NULL;
		fd = fopen("fat:/prii.log","ab");
		if(fd != NULL);
		{
			//0x0D0A = \r\n
			if(astr[strlen(astr)-1] == '\n' && astr[strlen(astr)-2] != '\r')
			{
				astr[strlen(astr)-1] = '\r';
				astr[strlen(astr)] = '\n';
				astr[strlen(astr)+1] = '\0';
			}
			fwrite(astr,1,strlen(astr),fd);
			fclose(fd);
		}
	}
	return;
}
void SetDumpDebug( u8 value )
{
	if (value != 1 && value != 0)
	{
		DumpDebug = 0;
		return;
	}
	DumpDebug = value;
	if (DumpDebug > 0 && GetMountedValue() > 0)
	{
		//create file, or re-open and add lining
		FILE* fd = NULL;
		fd = fopen("fat:/prii.log","rb");
		if(fd != NULL)
		{
			fclose(fd);
			fd = fopen("fat:/prii.log","ab");
			char str[] = "--------gecko_output_enabled------\r\n";
			fwrite(str,1,strlen(str),fd);
			fclose(fd);
		}
		else
		{
			//file didn't exist
			fclose(fd);
			fd = fopen("fat:/prii.log","wb");
			if(fd != NULL)
				fclose(fd);
		}
		
	}
	return;
}
void InitGDBDebug( void )
{
	DEBUG_Init(GDBSTUB_DEVICE_USB, 1);
	return;
}
