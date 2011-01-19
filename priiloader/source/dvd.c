/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2009 DacoTaco

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
#include "mem2_manager.h"
#include "dvd.h"

static s8 async_called = 0;
static u8 *inbuf = 0;
static u8 *outbuf = 0;
static s32 di_fd = 0;
DVD_status DVD_state;
static s32 SetDriveState (s32 result,void *usrdata)
{
	if (result != 0)
	{
		DVD_state.DriveError = 1;
	}
	else
	{
		DVD_state.DriveClosed = 1;
	}
	return 1;
}
static s32 GetDvdFD ( void )
{
	return di_fd;
}
void DVDStopDisc( bool do_async )
{
	if(async_called == 1) // async was called so this function is useless to do again
		return;

	DVD_state.DriveClosed = 0;
	DVD_state.DriveError = 0;
	if(di_fd == 0)
		di_fd = IOS_Open("/dev/di",0);
	if(di_fd)
	{
		if(inbuf == NULL)
			inbuf = (u8*)mem_align( 32, 0x20 );
		if(outbuf == NULL)
			outbuf = (u8*)mem_align( 32, 0x20 );

		memset(inbuf, 0, 0x20 );
		memset(outbuf, 0, 0x20 );

		((u32*)inbuf)[0x00] = 0xE3000000;
		((u32*)inbuf)[0x01] = 0;
		((u32*)inbuf)[0x02] = 0;

		DCFlushRange(inbuf, 0x20);
		//why crediar used an async and not look if it really closed (or was busy closing) is beyond me...
		if(!do_async)
		{
			IOS_Ioctl( di_fd, 0xE3, inbuf, 0x20, outbuf, 0x20);
			DVDCleanUp();
		}
		else
		{
			async_called = 1;
			IOS_IoctlAsync( di_fd, 0xE3, inbuf, 0x20, outbuf, 0x20, SetDriveState, NULL);
		}
	}
	return;
}
void DVDCleanUp( void )
{
	IOS_Close(di_fd);
	mem_free( outbuf );
	mem_free( inbuf );
	return;
}
s8 DvdKilled( void )
{
	if(async_called == 0) // no async was called so this function is useless
		return 1;
	if(DVD_state.DriveError)
	{
		DVDCleanUp();
		DVDStopDisc(false);
		DVD_state.DriveError = 0;
		DVD_state.DriveClosed = 1;
		async_called = 0;
		return 1;
	}
	if(DVD_state.DriveClosed)
	{
		DVDCleanUp();
		async_called = 0;
		return 1;
	}
	return 0;
}
