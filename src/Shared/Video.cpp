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

//Console height is the minimum height minus the first line (overscan)
#define CONSOLE_HEIGHT		(480-32)
#define CONSOLE_WIDTH		(640)

const static void _configureVideoMode(GXRModeObj* videoMode, bool initConsole)
{
	//if xfb is already set, that means we already configured
	//we reset the whole thing, just in case
	if (xfb)
		ShutdownVideo();

	//init video
	rmode = videoMode;
	VIDEO_Init();	

	//setup view size
	rmode->viWidth = 672;

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
	if(initConsole)
		CON_Init(xfb,(rmode->viWidth + rmode->viXOrigin - CONSOLE_WIDTH) / 2, (rmode->viHeight + rmode->viYOrigin - CONSOLE_HEIGHT) / 2, CONSOLE_WIDTH, CONSOLE_HEIGHT, CONSOLE_WIDTH*VI_DISPLAY_PIX_SZ );

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

void InitVideoWithConsole(void)
{
	if (_videoInit == true)
		return;

	_configureVideoMode(VIDEO_GetPreferredMode(NULL), true);
	return;
}

void InitVideo (void)
{
	if (_videoInit == true)
		return;

	_configureVideoMode(VIDEO_GetPreferredMode(NULL), false);
	return;
}

void ConfigureVideoMode(GXRModeObj* videoMode)
{
	_configureVideoMode(videoMode, false);
	return;
}
