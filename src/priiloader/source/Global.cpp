/*

priiloader(preloader mod) - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2013-2019  DacoTaco

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

//global functions,defines & variables for priiloader
#include "Global.h"
#include "Video.h"
#include "settings.h"
#include "gecko.h"

void ClearScreen()
{
	if (!SGetSetting(SETTING_BLACKBACKGROUND))
		VIDEO_ClearFrameBuffer(rmode, xfb, 0xFF80FF80);
	else
		VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	VIDEO_WaitVSync();
	printf("\x1b[2J");
	fflush(stdout);
	return;	
}

s8 InitNetwork()
{
	s32 result;
	gprintf("InitNetwork : Waiting for network init...");
	while ((result = net_init()) == -EAGAIN) {}
	if (result < 0)
	{
		gprintf("InitNetwork : DHCP timeout or no known access point available");
		return -2;
	}

    char myIP[16];
	//s32 if_config( char *local_ip, char *netmask, char *gateway,bool use_dhcp, int max_retries);
	if (if_config(myIP, NULL, NULL, true,1) < 0) 
	{
		gdprintf("InitNetwork : Error reading IP address");
		return -1;
	}
	gdprintf("InitNetwork : IP Address -> %s",myIP);
	return 1;
}
