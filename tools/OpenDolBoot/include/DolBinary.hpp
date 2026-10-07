/*
OpenDolBoot - An utiltiy to make a dol binary into a title bootable binary on the Wii/vWii

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

#include <cstddef>
#include <string>
#include <string.h>
#include <unistd.h>
#include <vector>

#include "Common.hpp"
#include "FileInfo.hpp"

#define MAX_TEXT_SECTIONS 7
#define MAX_DATA_SECTIONS 11

typedef struct
{
	unsigned int offsetText[MAX_TEXT_SECTIONS];
	unsigned int offsetData[MAX_DATA_SECTIONS];
	unsigned int addressText[MAX_TEXT_SECTIONS];
	unsigned int addressData[MAX_DATA_SECTIONS];
	unsigned int sizeText[MAX_TEXT_SECTIONS];
	unsigned int sizeData[MAX_DATA_SECTIONS];
	unsigned int addressBSS;
	unsigned int sizeBSS;
	unsigned int entrypoint;
	int padding1[7];
} dolHeader;

static_assert(sizeof(dolHeader) == 0x100);

const unsigned int NandLoaderLocation = 0x80003400;

class DOLSection
{
public:
	unsigned int Address;
	unsigned int Size;
	std::size_t Offset;
	unsigned char Index;
	const unsigned char *Data;

	bool IsAligned() const
	{
		return ALIGN32(Address) == Address && ALIGN32(Size) == Size;
	}

	bool IsNandLoaderSection() const
	{
		return (Address | 0x80000000) == NandLoaderLocation;
	}
};

class DolBinary : public FileInfo
{
public:
	std::vector<DOLSection> TextSections;
	std::vector<DOLSection> DataSections;
	unsigned int GetEntryPoint();
	unsigned int GetBSSAddress();
	unsigned int GetBSSSize();

	explicit DolBinary(const std::string &filename);
	~DolBinary() {}

	void Validate(bool throwOnError);
};