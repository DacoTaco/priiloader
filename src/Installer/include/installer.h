/*
Preloader/Priiloader Installer - An installation utiltiy for preloader (c) 2008-2020 crediar

Copyright (c) 2020 DacoTaco

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#pragma once

#ifndef ALIGN32
#define ALIGN32(x) (((x) + 31) & ~31)
#endif

#include <gctypes.h>

enum InstallerAction {
	None = 0,
	Install = 1,
	Update = 2,
	Remove = 3,
};

void InitializeInstaller(u64 titleId, bool isvWii);
bool PriiloaderInstalled(void);
void DeletePriiloaderFiles(InstallerAction action);
s32 RemovePriiloader(void);
s32 WritePriiloader(InstallerAction action);
s32 CopyTicket(void);
s32 PatchTMD(InstallerAction action);