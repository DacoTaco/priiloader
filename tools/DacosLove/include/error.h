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
#include "Global.h"

enum {
	ERROR_NONE,
	ERROR_UPDATE,
	ERROR_MALLOC,
	ERROR_SYSMENU_GENERAL,
	ERROR_SYSMENU_TIKNOTFOUND,
	ERROR_SYSMENU_TIKSIZEGETFAILED,
	ERROR_SYSMENU_TIKREADFAILED,
	ERROR_SYSMENU_ESDIVERFIY_FAILED,
	ERROR_SYSMENU_IOSSTUB,
	ERROR_SYSMENU_GETTMDSIZEFAILED,
	ERROR_SYSMENU_GETTMDFAILED,
	ERROR_SYSMENU_BOOTNOTFOUND,
	ERROR_SYSMENU_BOOTOPENFAILED,
	ERROR_SYSMENU_BOOTREADFAILED,
	ERROR_SYSMENU_BOOTGETSTATS,
	ERROR_THREAD_CREATE,
	ERROR_ISFS_INIT,
	ERROR_BOOT_DOL_OPEN,
	ERROR_BOOT_DOL_READ,
	ERROR_BOOT_DOL_SEEK,
	ERROR_BOOT_DOL_ENTRYPOINT,
	ERROR_BOOT_ERROR,
	ERROR_BOOT_HBC,
	ERROR_BOOT_BOOTMII,
	ERROR_STATE_CLEAR,
	ERROR_SETTING_OPEN,
	ERROR_SETTING_WRITE,
	ERROR_SETTING_READ,
	ERROR_HACKS_TO_LONG
};

extern u8 error;

void ShowError ( void );
