/*
bootloader - An utiltiy to make a dol binary bootable from a loader using the priiloader loader

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
#include <string>
#include <vector>

#include "executables.h"

#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep(x*1000)
#define SwapEndian(x) _byteswap_ulong(x)
#define SwapEndian16(x) _byteswap_ushort(x)
#else
#define SwapEndian(x) __builtin_bswap32(x)
#define SwapEndian16(x) __builtin_bswap16(x)
#include <unistd.h>
#include <string.h>
#endif

#define ALIGN32(x) (((x) + 31) & ~31)
#define ALIGN16(x) (((x) + 15) & ~15)

typedef struct {
	std::string Filename;
	unsigned int FileSize;
	unsigned char* Data;
} FileInfo;

extern const char loader[];
extern int loaderSize;

int WriteFile(FileInfo* info)
{
	if (info == NULL)
		return -1;

	FILE* file;
	file = fopen(info->Filename.c_str(), "wb+");
	if (!file)
		return -2;

	fwrite(info->Data, 1, info->FileSize, file);
	fclose(file);
	return 1;
}

int ReadFile(FileInfo* info)
{
	if (info == NULL || info->Filename.size() == 0)
		return -1;

	FILE* file;
	unsigned char* data = NULL;
	unsigned int size = 0;
#ifdef DEBUG
	printf("reading %s ...\r\n", info.Filename.c_str());
#endif

	file = fopen(info->Filename.c_str(), "rb");
	if (!file)
		return -2;

	fseek(file, 0, SEEK_END);
	size = ftell(file);
	rewind(file);

	// allocate memory to contain the whole file:
	data = (unsigned char*)malloc(size);
	if (data == NULL)
	{
		printf("Memory error\r\n");
		fclose(file);
		return -3;
	}
	memset(data, 0, size);
	//copy the file into the buffer:
	if (fread(data, 1, size, file) != size)
	{
		fclose(file);
		free(data);
		return -4;
	}
	fclose(file);

	info->Data = data;
	info->FileSize = size;

	return 1;
}

void ShowHelp()
{
	printf("bootloader Input_dol_filename Output_dol_filename \r\n\r\n");
	exit(0);
}

int main(int argc, char** argv)
{
	FileInfo outputFileInfo;
	FileInfo inputFileInfo;
	std::vector<std::string> argumentList;

	printf("bootloader - tool to add a bootloader to a wii binary\r\n\r\n");
	if (argc != 3)
	{
		ShowHelp();
	}

	//load arguments except for the first, which is just the executable path
	for (int i = 1; i < argc; i++)
	{
		argumentList.push_back(argv[i]);
	}

	for (unsigned int i = 0; i < argumentList.size(); i++)
	{
		std::string argument = argumentList[i];
		if (argument[0] == '-')
		{
			if (argument == "-h") // -h / help
			{
				ShowHelp();
			}
		}
		else if (inputFileInfo.Filename.size() == 0)
		{
			inputFileInfo.Filename = argument;
		}
		else if (outputFileInfo.Filename.size() == 0)
		{
			outputFileInfo.Filename = argument;
		}
		else //there was an unexpected parameter
		{
			printf("unexpected arg '%s'\r\n", argument.c_str());
			ShowHelp();
		}
	}

	if (inputFileInfo.Filename.size() == 0 || outputFileInfo.Filename.size() == 0)
		ShowHelp();

	try
	{
		if(ReadFile(&inputFileInfo) < 0 || inputFileInfo.Data == NULL || inputFileInfo.FileSize == 0)
			throw "failed to read input file";

		const unsigned int baseAddress = 0x80003400;
		unsigned int _startup[] = {
			//setup parameters for loader
			0x3c608000, //lis 3,0x80003400@h
			0x60633400, //ori 3,3,0x80003400@l
			0x38800000, //li 4,0
			0x38a00000, //li 5,0
			0x38c00000, //li 6,0
			//jump to loader
			0x3d008000, //lis 8,0x80003430@h
			0x61083400, //ori 8,8,0x80003430@l
			0x7d0903a6, //mtctr 8
			0x7d0803a6, //mtlr 8
			0x4e800020, //blr
			0x00000000, //padding
			0x00000000, //padding
		};

		outputFileInfo.FileSize = ALIGN32(sizeof(dolhdr)) + sizeof(_startup) + inputFileInfo.FileSize + ALIGN16(loaderSize);
		outputFileInfo.Data = (unsigned char*)malloc(outputFileInfo.FileSize);
		if (outputFileInfo.Data == NULL)
			throw "failed to allocate memory";

		memset(outputFileInfo.Data, 0, outputFileInfo.FileSize);

		//add entrypoint
		dolhdr* dolHdr = (dolhdr*)outputFileInfo.Data;
		dolHdr->entrypoint = SwapEndian(baseAddress);
		dolHdr->addressText[0] = SwapEndian(baseAddress);
		dolHdr->offsetText[0] = SwapEndian(ALIGN32(sizeof(dolhdr)));
		dolHdr->sizeText[0] = SwapEndian(sizeof(_startup));

		//add loader
		unsigned int loaderAddress = SwapEndian(dolHdr->addressText[0]) + SwapEndian(dolHdr->sizeText[0]);
		dolHdr->addressText[1] = SwapEndian(loaderAddress);
		dolHdr->offsetText[1] = SwapEndian(SwapEndian(dolHdr->offsetText[0]) + SwapEndian(dolHdr->sizeText[0]));
		dolHdr->sizeText[1] = SwapEndian(ALIGN16(loaderSize));
		memcpy(&outputFileInfo.Data[SwapEndian(dolHdr->offsetText[1])], loader, loaderSize);

		//add input file
		unsigned int binaryAddress = SwapEndian(dolHdr->addressText[1]) + SwapEndian(dolHdr->sizeText[1]);
		dolHdr->addressData[0] = SwapEndian(binaryAddress);
		dolHdr->offsetData[0] = SwapEndian(SwapEndian(dolHdr->offsetText[1]) + SwapEndian(dolHdr->sizeText[1]));
		dolHdr->sizeData[0] = SwapEndian(inputFileInfo.FileSize);
		memcpy(&outputFileInfo.Data[SwapEndian(dolHdr->offsetData[0])], inputFileInfo.Data, inputFileInfo.FileSize);

		//calculate & set binary/loader addresses in startup		
		_startup[0] = (_startup[0] & 0xFFFF0000) | (binaryAddress >> 16);
		_startup[1] = (_startup[1] & 0xFFFF0000) | (binaryAddress & 0x0000FFFF);
		_startup[5] = (_startup[5] & 0xFFFF0000) | (loaderAddress >> 16);
		_startup[6] = (_startup[6] & 0xFFFF0000) | (loaderAddress & 0x0000FFFF);

		//copy data, can't use memcpy as we need to flip endianness...
		unsigned int* data = (unsigned int*)&outputFileInfo.Data[ALIGN32(sizeof(dolhdr))];
		for (int i = 0; i < (sizeof(_startup) / sizeof(unsigned int)); i++)
			data[i] = SwapEndian(_startup[i]);

		WriteFile(&outputFileInfo);
		free(outputFileInfo.Data);
		free(inputFileInfo.Data);
	}
	catch (const std::string& ex)
	{
		printf("bootloader Exception -> %s\r\n", ex.c_str());
		exit(1);
	}
	catch (char const* ex)
	{
		printf("bootloader Exception -> %s\r\n", ex);
		exit(1);
	}
	catch (...)
	{
		printf("bootloader Exception was thrown\r\n");
		exit(1);
	}

	return 0;
}