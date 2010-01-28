/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

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
#include <gccore.h>
#include <string.h>
#include <malloc.h>

#include "error.h"
#include "settings.h"
#include "font.h"

u8 error = 0;

void ShowError ( int height , int width )
{
	if( error >= 0 )
	{
		if( SGetSetting( SETTING_LIDSLOTONERROR ) )
			*(vu32*)0xCD8000C0 |= 0x20;

		switch( error )
		{
			case ERROR_NONE:
			/*default:*/
			break;
			case ERROR_BOOT_DOL_OPEN:
				PrintFormat( 0, 16, height-128, "Error autobooting file, try reinstalling!");
				PrintFormat( 0, 16, height-112, "Could not open file!");
			break;
			case ERROR_BOOT_DOL_READ:
				PrintFormat( 0, 16, height-128, "Error autobooting file, try reinstalling!");
				PrintFormat( 0, 16, height-112, "Reading the file failed!");
			break;
			case ERROR_BOOT_DOL_SEEK:
				PrintFormat( 0, 16, height-128, "Error autobooting file, try reinstalling!");
				PrintFormat( 0, 16, height-112, "Seek failed!");
			break;
			case ERROR_BOOT_DOL_ENTRYPOINT:
				PrintFormat( 0, 16, height-128, "Error autobooting file, try reinstalling!");
				PrintFormat( 0, 16, height-112, "Entrypoint is zero!");
			break;
			case ERROR_ISFS_INIT:
				PrintFormat( 0, 16, height-128, "ISFS_Initialize() failed");
			break;
			case ERROR_BOOT_HBC:
				PrintFormat( 0, 16, height-128, "Error autobooting HBC, maybe title not installed?");
			break;
			case ERROR_BOOT_BOOTMII:
				PrintFormat( 0, 16, height-128, "Error booting Bootmii IOS!");
			break;
			case ERROR_BOOT_ERROR:
				PrintFormat( 0, 16, height-128, "Error autobooting due problems with the settings.ini!");
			break;
			case ERROR_SETTING_OPEN:
				PrintFormat( 0, 16, height-128, "Problems with settings.ini!");
				PrintFormat( 0, 16, height-112, "Could not open/create file!");
			break;
			case ERROR_SETTING_WRITE:
				PrintFormat( 0, 16, height-128, "Problems with settings.ini!");
				PrintFormat( 0, 16, height-112, "Could not write file!");
			break;
			case ERROR_SETTING_READ:
				PrintFormat( 0, 16, height-128, "Problems with settings.ini!");
				PrintFormat( 0, 16, height-112, "Could not read file!");
			break;
			case ERROR_THREAD_CREATE:
				PrintFormat( 0, 16, height-128, "LWP_CreateThread() failed!");
			break;
			case ERROR_MALLOC:
				PrintFormat( 0, 16, height-128, "malloc failed!");
			break;
			case ERROR_STATE_CLEAR:
				PrintFormat( 0, 16, height-128, "failed to clear state!");
			break;
			case ERROR_SYSMENU_GENERAL:
				PrintFormat( 0, 16, height-128, "Failed to load system menu!");
			break;
			case ERROR_SYSMENU_TIKNOTFOUND:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "Ticket not found!");
			break;
			case ERROR_SYSMENU_TIKSIZEGETFAILED:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "Could not get ticket size!");
			break;
			case ERROR_SYSMENU_TIKREADFAILED:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "Could not read ticket!");
			break;
			case ERROR_SYSMENU_ESDIVERFIY_FAILED:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "ES_DiVerfiy failed! Is the IOS patched?");
			break;
			case ERROR_SYSMENU_IOSSTUB:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "The going to load IOS was detected as Stub!");
			break;
			case ERROR_SYSMENU_GETTMDSIZEFAILED:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "Could not get TMD size!");
			break;
			case ERROR_SYSMENU_GETTMDFAILED:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "Could not get TMD!");
			break;
			case ERROR_SYSMENU_BOOTNOTFOUND:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "Boot file not found!");
			break;
			case ERROR_SYSMENU_BOOTOPENFAILED:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "Could not open boot file!");
			break;
			case ERROR_SYSMENU_BOOTGETSTATS:
				PrintFormat( 0, 16, height-128, "Error autobooting systemmenu!");
				PrintFormat( 0, 16, height-112, "Could not get boot stats!");
			break;
			/*
			default:
				PrintFormat( 0, 16, height-128, "Unknown error:%d\n", error);
				break;*/
		}
	}
}
