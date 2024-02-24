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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <memory>

#include "../include/NandLoaderInjector.hpp"
#include "../include/NandLoader.h"
#include "../include/dolHeader.h"
#include "../include/nandloader.bin.h"

const std::string internalFileName = "internal";
const unsigned int nandLoaderLocation = 0x80003400;

void NandLoaderInjector::InjectNandLoader(std::unique_ptr<FileInfo>& input, std::unique_ptr<FileInfo>& nandLoader, std::unique_ptr<FileInfo>& output)
{
	printf("input : %s\n",input->GetFilename());
	printf("output : %s\n", output->GetFilename());
	printf("nandloader : %s\n", nandLoader->GetFilename());	

	const auto headerSize = sizeof(dolHeader);
	auto inputHeader = (dolHeader*)&input->Data[0];
	if(inputHeader->sizeText[6] || inputHeader->addressText[6] || inputHeader->offsetText[6])
		throw "All text segments already contains data! quiting out of failsafe...";

	for(auto i = 0; i < 6;i++)
	{
		if(ForceBigEndian(inputHeader->addressText[i]) == nandLoaderLocation)
			throw "Binary already contains nandloader";
	}

	NandLoader* loader = (NandLoader*)&nandLoader->Data[0];
	if (ForceBigEndian(loader->Identifier) == NANDLDR_MAGIC && (ForceBigEndian(loader->Entrypoint) != ForceBigEndian(inputHeader->entrypoint)))
	{
		printf("different nboot to dol entrypoint detected! Changing\n\t0x%08X\tto\t0x%08X\n", ForceBigEndian(loader->Entrypoint), ForceBigEndian(inputHeader->entrypoint));
		loader->Entrypoint = inputHeader->entrypoint;
	}
	
	//copy header data
  	std::copy(input->Data.begin(), input->Data.begin() + headerSize, std::back_inserter(output->Data));
	
	//copy in nandloader & set header
	std::copy(nandLoader->Data.begin(), nandLoader->Data.end(), std::back_inserter(output->Data));

	//copy in other binary data
	std::copy(input->Data.begin() + headerSize, input->Data.end(), std::back_inserter(output->Data));

	//set output header
	dolHeader* outputHeader = (dolHeader*)&output->Data[0];
	outputHeader->addressText[0] = ForceBigEndian(nandLoaderLocation);
	outputHeader->offsetText[0] = ForceBigEndian(headerSize);
	outputHeader->sizeText[0] = ForceBigEndian(nandLoader->GetFileSize());
	for(int i = 0;i < 6;i++)
	{
		if(!inputHeader->sizeText[i] || !inputHeader->addressText[i] || !inputHeader->offsetText[i])
			continue;

		//valid info. move it over
		printf("moving text section #%d...\n",i);
		outputHeader->addressText[i+1] = inputHeader->addressText[i];
		outputHeader->sizeText[i+1] = inputHeader->sizeText[i];
		outputHeader->offsetText[i+1] = SwapEndian(SwapEndian(inputHeader->offsetText[i]) + nandLoader->GetFileSize());
	}

	for(int i = 0;i <= 10;i++)
	{
		if(!inputHeader->sizeData[i] || !inputHeader->addressData[i] || !inputHeader->offsetData[i])
			continue;

		//valid info. move it over
		printf("copying data section #%d...\n",i);
		outputHeader->addressData[i] = inputHeader->addressData[i];
		outputHeader->sizeData[i] = inputHeader->sizeData[i];
		outputHeader->offsetData[i] = SwapEndian(SwapEndian(inputHeader->offsetData[i]) + nandLoader->GetFileSize());
	}
}

void NandLoaderInjector::InjectNandLoader(std::unique_ptr<FileInfo>& input, std::unique_ptr<FileInfo>& output)
{
	auto nandLoader = std::make_unique<FileInfo>(internalFileName, nandloader_bin, nandloader_bin_size);
	NandLoaderInjector::InjectNandLoader(input, nandLoader, output);
}