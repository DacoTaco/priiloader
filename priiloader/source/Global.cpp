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
#include "settings.h"
#include "gecko.h"

GXRModeObj *rmode = NULL;
void *xfb = NULL;
static u8 vid_init = 0;

void _configureVideoMode(GXRModeObj* videoMode, s8 internalConfig)
{
	rmode = videoMode;

	//if xfb is already set, that means we already configured
	//we reset the whole thing, just in case
	if (xfb)
	{
		free(MEM_K1_TO_K0(xfb));
		//de-init video
		vu16* const _viReg = (u16*)0xCC002000;
		if ((_viReg[1] & 0x0001))
		{
			//reset & de-init the regs. at least that should work according to libogc
			u32 cnt = 0;
			_viReg[1] = 0x02;
			while (cnt < 1000) cnt++;
			_viReg[1] = 0x00;
		}

		//init video
		VIDEO_Init();
	}

	//apparently the video likes to be bigger then it actually is on NTSC/PAL60/480p. lets fix that!
	if ((internalConfig) &&
		(rmode->viTVMode == VI_NTSC || CONF_GetEuRGB60() || CONF_GetProgressiveScan()))
	{
		//the correct one would be * 0.035 to be sure to get on the Action safe of the screen.
		GX_AdjustForOverscan(rmode, rmode, 0, rmode->viWidth * 0.035);
	}

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
}
void InitVideo ( void )
{
	if (vid_init == 1)
		return;

	VIDEO_Init();
	_configureVideoMode(VIDEO_GetPreferredMode(NULL), 1);
	vid_init = 1;
	gdprintf("resolution is %dx%d",rmode->viWidth,rmode->viHeight);
	return;
}

void ClearScreen()
{
	if (!SGetSetting(SETTING_BLACKBACKGROUND))
		VIDEO_ClearFrameBuffer(rmode, xfb, 0xFF80FF80);
	else
		VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	VIDEO_WaitVSync();
	printf("\x1b[5;0H");
	fflush(stdout);
	return;
}

void ConfigureVideo(GXRModeObj* videoMode)
{
	_configureVideoMode(videoMode, 0);
}

s8 InitNetwork()
{
	s32 result;
	gprintf("InitNetwork : Waiting for network to initialise...");
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
