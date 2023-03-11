/*
Videop.cpp - Wii Video functionality

Copyright (C) 2023
DacoTaco

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

#include <malloc.h>
#include <gctypes.h>
#include <gccore.h>
#include <ogc/machine/processor.h>

#include "vWii.h"
#include "Video.h"

void* xfb = NULL;
GXRModeObj* rmode;
static bool _videoInit = false;

const static void _configureVideoMode(GXRModeObj* videoMode, s8 internalConfig)
{
	//if xfb is already set, that means we already configured
	//we reset the whole thing, just in case
	if (xfb)
		ShutdownVideo();

	//init video
	rmode = videoMode;
	VIDEO_Init();	

	//setup view size & DMCU
	rmode->viWidth = 672;
	if(CheckvWii())
	{
		//control WiiU's DMCU to turn off(widescreen) or on(regular) pillarboxing
		write32(0x0d8006a0, CONF_GetAspectRatio() == CONF_ASPECT_16_9 ? 0x30000004 : 0x10000002);
		mask32(0x0d8006a8, 0, 2);
	}

	//set correct middlepoint of the screen
	if (rmode == &TVPal576IntDfScale || rmode == &TVPal576ProgScale) 
	{
		rmode->viXOrigin = (VI_MAX_WIDTH_PAL - rmode->viWidth) / 2;
		rmode->viYOrigin = (VI_MAX_HEIGHT_PAL - rmode->viHeight) / 2;
	} 
	else 
	{
		rmode->viXOrigin = (VI_MAX_WIDTH_NTSC - rmode->viWidth) / 2;
		rmode->viYOrigin = (VI_MAX_HEIGHT_NTSC - rmode->viHeight) / 2;
	}

	xfb = memalign(32, VIDEO_GetFrameBufferSize(rmode) + 0x100);
	DCInvalidateRange(xfb, VIDEO_GetFrameBufferSize(rmode) + 0x100);
	xfb = MEM_K0_TO_K1(xfb);

	VIDEO_SetBlack(true);
	VIDEO_Configure(rmode);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	// Initialise the console, required for printf
	CON_InitEx(rmode, (rmode->viWidth + rmode->viXOrigin - 640) / 2, (rmode->viHeight + rmode->viYOrigin - 480) / 2, 640, 480);

	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();

	_videoInit = true;
}

void ShutdownVideo(void)
{
	//de-init video
	vu16* const _viReg = (u16*)0xCC002000;
	if ((_viReg[1] & 0x0001))
	{
		//reset & de-init the regs. at least that should work according to libogc
		u32 cnt = 0;
		_viReg[1] = 0x02;
		while(cnt<1000) 
		{
			//empty asm so the compiler does not optimise this loop out
			__asm__ __volatile__ ("" ::: "memory");
			cnt++;
		}
		_viReg[1] = 0x00;
	}

	free(MEM_K1_TO_K0(xfb));
	_videoInit = false;
}

void InitVideo ( void )
{
	if (_videoInit == true)
		return;

	_configureVideoMode(VIDEO_GetPreferredMode(NULL), 1);
	return;
}

void ConfigureVideoMode(GXRModeObj* videoMode)
{
	_configureVideoMode(videoMode, 0);
	return;
}