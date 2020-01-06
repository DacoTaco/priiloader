/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019  crediar

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

#include "state.h"
#include "mem2_manager.h"

wii_state system_state;

s32 CheckBootState( void )
{
	StateFlags *sf = (StateFlags *)mem_align( 32,  ALIGN32(sizeof(StateFlags)) );
	if(sf == NULL)
		return 0;
	memset( sf, 0, sizeof(StateFlags) );
	s32 fd = ISFS_Open("/title/00000001/00000002/data/state.dat", 1);
	if(fd < 0)
	{
		mem_free(sf);
		return 0;
	}
	s32 ret = ISFS_Read(fd, sf, sizeof(StateFlags));
	ISFS_Close(fd);
	if(ret != sizeof(StateFlags))
	{
		mem_free(sf);
		return 0;
	}
	u8 r = sf->type;
	mem_free( sf );
	return r;
}
StateFlags GetStateFlags( void )
{
	StateFlags *sf = (StateFlags *)mem_align( 32, ALIGN32(sizeof(StateFlags)) );
	StateFlags State;
	memset( sf, 0, sizeof(StateFlags) );
	s32 fd = ISFS_Open("/title/00000001/00000002/data/state.dat", 1);
	if(fd < 0)
	{
		mem_free(sf);
		return State;
	}
	s32 ret = ISFS_Read(fd, sf, sizeof(StateFlags));
	ISFS_Close(fd);
	if(ret != sizeof(StateFlags))
	{
		mem_free(sf);
		return State;
	}
	memcpy((StateFlags*)&State,sf,sizeof(StateFlags));
	mem_free( sf );
	return State;
}
static u32 __CalcChecksum(u32 *buf, int len)
{
	u32 sum = 0;
	int i;
	len = (len/4);

	for(i=1; i<len; i++)
		sum += buf[i];
	return sum;
}
s32 ClearState( void )
{
	return SetBootState(0,0,0,0);

}
s32 SetBootState( u8 type , u8 flags , u8 returnto , u8 discstate )
{
	StateFlags *sf = (StateFlags *)mem_align( 32, sizeof(StateFlags) );
	memset( sf, 0, sizeof(StateFlags) );

	s32 fd = ISFS_Open("/title/00000001/00000002/data/state.dat", 1|2 );
	if(fd < 0)
	{
		mem_free( sf );
		ISFS_Close(fd);
		return -1;
	}
	
	s32 ret = ISFS_Read(fd, sf, sizeof(StateFlags));

	if(ret != sizeof(StateFlags))
	{
		mem_free( sf );
		ISFS_Close(fd);
		return -2 ;
	}

	sf->type = type;
	sf->returnto = returnto;
	sf->flags = flags;
	sf->discstate = discstate;
	sf->checksum= __CalcChecksum((u32*)sf, sizeof(StateFlags));

	if(ISFS_Seek( fd, 0, 0 )<0)
	{
		mem_free( sf );
		ISFS_Close(fd);
		return -3;
	}

	if(ISFS_Write(fd, sf, sizeof(StateFlags))!=sizeof(StateFlags))
	{
		mem_free( sf );
		ISFS_Close(fd);
		return -4;
	}

	ISFS_Close(fd);

	mem_free( sf );
	return 1;

}
s8 VerifyNandBootInfo ( void )
{
	// path : /shared2/sys/NANDBOOTINFO
	NANDBootInfo *Boot_Info = (NANDBootInfo *)mem_align( 32, sizeof(NANDBootInfo) );
	memset( Boot_Info, 0, sizeof(NANDBootInfo) );

	s32 fd = ISFS_Open("/shared2/sys/NANDBOOTINFO", 1 );
	if(fd < 0)
	{
		mem_free( Boot_Info );
		return -1;
	}
	
	s32 ret = ISFS_Read(fd, Boot_Info, sizeof(NANDBootInfo));
	if(ret != sizeof(NANDBootInfo))
	{
		ISFS_Close(fd);
		mem_free( Boot_Info );
		return -2 ;
	}
	ISFS_Close(fd);

	u8 r = Boot_Info->titletype;
	mem_free( Boot_Info );

	if (r == 8)
	{
		SetBootState(4,132,0,0);
		return 1;
	}
	else
		return 0;
}
s32 SetNandBootInfo(void)
{
	static NANDBootInfo BootInfo ATTRIBUTE_ALIGN(32);
	memset(&BootInfo,0,sizeof(NANDBootInfo));
	BootInfo.apptype = 0x80;
	BootInfo.titletype = 2;
	if(ES_GetTitleID(&BootInfo.launcher) < 0)
		BootInfo.launcher = 0x0000000100000002LL;
	BootInfo.checksum = __CalcChecksum((u32*)&BootInfo,sizeof(NANDBootInfo));
	s32 fd = ISFS_Open("/shared2/sys/NANDBOOTINFO", ISFS_OPEN_RW );
	if(fd < 0)
	{
		return fd;
	}
	s32 ret = ISFS_Write(fd, &BootInfo, sizeof(NANDBootInfo));
	if(ret < 0)
	{
		ISFS_Close(fd);
		gprintf("SetNandBootInfo : ISFS_Write returned %d\r\n",ret);
		return -2;
	}
	ISFS_Close(fd);
	return 1;
}
