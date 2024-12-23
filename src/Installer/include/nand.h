/*
Preloader/Priiloader Installer - An installation utiltiy for preloader (c) 2008-2020 crediar

Copyright (c) 2024 DacoTaco

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

#include <gctypes.h>
#include <string>

enum NandActionErrors
{
	InvalidArgument = -80,
	MemoryAllocation = -81,
	HashCalculation = -82,
	HashComparison = -83,
	RenameFailure = -84,
};

typedef struct
{
	u32 owner;
	u16 group;
	u8 attributes;
	u8 ownerperm;
	u8 groupperm;
	u8 otherperm;
} NandPermissions;

s32 NandWrite(const std::string destination, const void* data, u32 dataSize, NandPermissions destPermissions);
s32 NandCopy(const std::string source, const std::string destination, NandPermissions srcPermissions);