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
#include <string>
#include <vector>
#include "../include/OpenDolBoot.h"

void _ShowHelp()
{
	printf("OpenDolBoot Input_Dol_filename [-i] [-n Nandcode_filename] [-h] Output_App_filename \n\n");
	printf("parameters:\n");
	printf("-i : display info about the dol file and exit (no other parameters are required when using -i)\n");
	printf("-n : use the following nand code and not the default nboot.bin\n");
	printf("-h : display this message\n");
	exit(0);
}
int writeFile(file_info* info)
{
	if(info == NULL)
		return -1;

	FILE* file;
	file = fopen(info->filename.c_str(),"wb+");
	if(!file)
		return -2;

	fwrite(info->data,1,info->file_size,file);
	fclose(file);
	return 1;
}
int readFile(std::string filename, file_info* info)
{
	if(filename.size() == 0 || info == NULL)
		return -1;
	
	FILE* file;
	unsigned char* data = NULL;
	unsigned int size = 0;
#ifdef DEBUG
	printf("reading %s ...\n",filename.c_str());
#endif
	file = fopen(filename.c_str(),"rb");
	if(!file)
		return -2;

	fseek(file , 0 , SEEK_END);
	size = ftell(file);
	rewind(file);

	// allocate memory to contain the whole file:
	data = (unsigned char*) malloc (size);
	if (data == NULL) 
	{
		printf("Memory error");
		return -3;
	}
	memset(data,0,size);
	//copy the file into the buffer:
	if (fread(data,1,size,file) != size) 
	{
		return -4;
	}
	fclose(file);	
	info->filename = filename;
	info->data = data;
	info->file_size = size;
	
	return 1;
}
int main(int argc, char **argv)
{
	printf("DacoTaco's OpenDolboot : Version %s\n\n",VERSION);
	if(argc < 3)
	{
		_ShowHelp();
	}
	std::string inputFile = "";
	std::string outputFile = "";
	std::string nandCodeFile = "";
	std::vector<std::string> argumentList;
	bool showInfo = false;
	//load arguments except for the first, which is just the executable path
	for(int i = 1; i < argc;i++)
	{
		argumentList.push_back(argv[i]);
	}	

	std::string prev_arg = "";
	for(unsigned int i = 0; i < argumentList.size();i++)
	{
		std::string argument = argumentList[i];
		if(argument[0] == '-')
		{
			if(argument == "-i")
				showInfo = true;
			else if(argument == "-h") // -h / help
			{
				_ShowHelp();
			}
			else if(argument != "-n") //all unknown arguments
			{
				printf("unknown arg '%s'\n",argument.c_str());
				_ShowHelp();
			}
		}
		else if(prev_arg == "-n")
		{
			if(nandCodeFile.size() > 0)
				_ShowHelp();

			nandCodeFile = argument;	
		}
		else if(inputFile.size() == 0)
		{
			inputFile = argument;
		}
		else if(showInfo == false && outputFile.size() == 0)
		{
			outputFile = argument;
		}
		else //there was an unexpected parameter
		{
			printf("unexpected arg '%s'\n",argument.c_str());
			_ShowHelp();
		}	

		prev_arg = argument;
	}

	if(inputFile.size() == 0 || (showInfo == false && outputFile.size() == 0))
		_ShowHelp();

	printf("input : %s\n",inputFile.c_str());
	if(!showInfo)
	{
		printf("output : %s\n",outputFile.c_str());
		if(nandCodeFile.size() > 0)
			printf("using nandcode file : %s\n",nandCodeFile.c_str());	
	}

	//get input file info
	dolhdr* inputHeader;
	dolhdr* outputHeader;
	file_info input_file;
	file_info output_file;
	if(readFile(inputFile,&input_file) < 0)
	{
		printf("failed to read input file\n");
		return 1;
	}
	inputHeader = (dolhdr*)input_file.data;

	if(showInfo)
	{
		printf("\n\n");
		printf("Entrypoint: 0x%08X\nBSS Address : 0x%08X\nBSS Size: 0x%08X\n\n", (unsigned int)SwapEndian(inputHeader->entrypoint) , (unsigned int)SwapEndian(inputHeader->addressBSS) , (unsigned int)SwapEndian(inputHeader->sizeBSS) );
		printf("Text Sections:\n");
		for(int i = 0;i < 6;i++)
		{
			if(inputHeader->sizeText[i] && inputHeader->addressText[i] && inputHeader->offsetText[i])
			{
				//valid info. swap and display
				printf("offset : 0x%08X\taddress : 0x%08X\tsize : 0x%08X\n", (unsigned int)SwapEndian(inputHeader->offsetText[i]), (unsigned int)SwapEndian(inputHeader->addressText[i]), (unsigned int)SwapEndian(inputHeader->sizeText[i]));
			}
		}
		printf("\nData Sections:\n");
		for(int i = 0;i <= 10;i++)
		{
			if(inputHeader->sizeData[i] && inputHeader->addressData[i] && inputHeader->offsetData[i])
			{
				//valid info. swap and display
				printf("offset : 0x%08X\taddress : 0x%08X\tsize : 0x%08X\n", (unsigned int)SwapEndian(inputHeader->offsetData[i]), (unsigned int)SwapEndian(inputHeader->addressData[i]), (unsigned int)SwapEndian(inputHeader->sizeData[i]));
			}
		}
		free(input_file.data);
		return 0;
	}

	file_info nand_info;
	if(nandCodeFile.size() > 0)
	{
		if(readFile(nandCodeFile,&nand_info) < 0)
		{
			printf("failed to read nand file\n");
			free(input_file.data);
			return 1;
		}
	}
	else
	{
		nand_info.filename = "internal";
		nand_info.file_size = _nboot_size;
		nand_info.data = (unsigned char*)_nboot;
	}

#ifdef DEBUG
	printf("doing insanity checks...\n");
#endif

	if(inputHeader->sizeText[1] && inputHeader->addressText[1] && inputHeader->offsetText[1])
	{
		//o ow, text2 is already full. lets quit before we brick ppl
		printf("Text2 already contains data! quiting out of failsafe...\n");
		free(input_file.data);
		return 1;
	}
	if(nandCodeFile.size() == 0)
	{
		Nandcode* nboot = (Nandcode*)nand_info.data;
		if (SwapEndian16(nboot->upper_entrypoint) != ( SwapEndian(inputHeader->entrypoint) >> 16) || SwapEndian16(nboot->lower_entrypoint) != ( SwapEndian(inputHeader->entrypoint) & 0x0000FFFF ))
		{
			printf("different nboot to dol entrypoint detected! Changing\n\t0x%04X%04X\tto\t0x%04X%04X\n",
				SwapEndian16(nboot->upper_entrypoint),SwapEndian16(nboot->lower_entrypoint),
				SwapEndian(inputHeader->entrypoint) >> 16,SwapEndian(inputHeader->entrypoint)& 0x0000FFFF);
			nboot->upper_entrypoint = (unsigned short)(SwapEndian(inputHeader->entrypoint) >> 16);
			nboot->lower_entrypoint = (unsigned short)(SwapEndian(inputHeader->entrypoint) & 0x0000FFFF);
		}
	}

	//set & allocate new file data
	output_file.file_size = input_file.file_size + nand_info.file_size;
	output_file.filename = outputFile;
	output_file.data = (unsigned char*)malloc(output_file.file_size);
	if(output_file.data == NULL)
	{
		printf("failed to alloc output file\n");
		free(input_file.data);
		return 1;
	}
	memset(output_file.data,0,output_file.file_size);
	outputHeader = (dolhdr*)output_file.data;
	memcpy(outputHeader,inputHeader,sizeof(dolhdr));


	//set dolheader data
	outputHeader->addressText[0] = SwapEndian(0x80003400);
	outputHeader->offsetText[0] = SwapEndian(0x00000100);
	outputHeader->sizeText[0] = SwapEndian(nand_info.file_size);
	for(int i = 0;i < 5;i++)
	{
		if(inputHeader->sizeText[i] && inputHeader->addressText[i] && inputHeader->offsetText[i])
		{
			//valid info. move it over
			printf("moving text section #%d...\n",i);
			outputHeader->addressText[i+1] = inputHeader->addressText[i];
			outputHeader->sizeText[i+1] = inputHeader->sizeText[i];
			outputHeader->offsetText[i+1] = inputHeader->offsetText[i] + SwapEndian(nand_info.file_size);
		}
	}

	for(int i = 0;i <= 10;i++)
	{
		if(inputHeader->sizeData[i] && inputHeader->addressData[i] && inputHeader->offsetData[i])
		{
			//valid info. move it over
			printf("moving data section #%d...\n",i);
			outputHeader->addressData[i] = inputHeader->addressData[i];
			outputHeader->sizeData[i] = inputHeader->sizeData[i];
			outputHeader->offsetData[i] = inputHeader->offsetData[i] + SwapEndian(nand_info.file_size);
		}
	}
	printf("copying binary data...\n");
	memcpy(output_file.data + 0x100,nand_info.data,nand_info.file_size);
	memcpy(	output_file.data + SwapEndian(outputHeader->offsetText[1]),
			input_file.data + SwapEndian(inputHeader->offsetText[0]),
			input_file.file_size - sizeof(dolhdr));


	if(writeFile(&output_file))
	{
		printf("Done! check %s to verify!\n",output_file.filename.c_str());
	}
	else
	{
		printf("failed to write new data to %s\n",output_file.filename.c_str());
	}

	free(input_file.data);
	free(output_file.data);
	return 0;
}
