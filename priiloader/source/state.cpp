/*

preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2009  crediar

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
#include <unistd.h>
#include <gccore.h>
#include <string.h>
#include <malloc.h>

#include "state.h"

s32 CheckBootState( void )
{
	//the memalign has random crashes -> short on memory?
	StateFlags *sf = (StateFlags *)memalign( 32, sizeof(StateFlags) );
	memset( sf, 0, sizeof(StateFlags) );
	s32 fd = ISFS_Open("/title/00000001/00000002/data/state.dat", 1);
	if(fd < 0)
		return 0;
	s32 ret = ISFS_Read(fd, sf, sizeof(StateFlags));
	IOS_Close(fd);
	if(ret != sizeof(StateFlags))
	{
		return 0;
	}
	s32 r = sf->type;
	free( sf );
	return r;
}
u32 CalcStateChk(u32 *buf, u32 size)
{
	u32 chksum = 0;

	for( u32 i=1; i<size>>2; ++i )
		chksum += buf[i];

	return chksum;
}
s32 ClearState( void )
{
	StateFlags *sf = (StateFlags *)memalign( 32, sizeof(StateFlags) );
	memset( sf, 0, sizeof(StateFlags) );

	s32 fd = ISFS_Open("/title/00000001/00000002/data/state.dat", 1|2 );
	if(fd < 0)
		return -1;
	
	s32 ret = ISFS_Read(fd, sf, sizeof(StateFlags));

	if(ret != sizeof(StateFlags))
		return -2 ;

	sf->type = 0;
	sf->returnto = 0;
	sf->flags = 0;
	sf->checksum= CalcStateChk( (u32*)sf, sizeof(StateFlags) );

	if(ISFS_Seek( fd, 0, 0 )<0)
		return -3;

	if(ISFS_Write(fd, sf, sizeof(StateFlags))!=sizeof(StateFlags))
		return -4;

	IOS_Close(fd);
	//not sure if should free this or not...
	//free(sf);
	return 1;

}
