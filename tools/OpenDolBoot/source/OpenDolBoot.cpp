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
#include <unistd.h>
#include "../include/dolHeader.h"
#include "../include/FileInfo.hpp"
#include "../include/NandLoaderInjector.hpp"
#include "../include/nandloader.bin.h"

const std::string internalFileName = "internal";
const unsigned int nandLoaderLocation = 0x80003400;

void ShowHelp()
{
	printf("OpenDolBoot [input] [options] [output] \n\n");
	printf("parameters:\n");
	printf("-i\t\t: display info about the dol file and exit (no other parameters are required when using -i)\n");
	printf("-n <nandcode>\t: use the following nand code and not the default nboot.bin\n");
	printf("-h\t\t: display this message\n");
	printf("-f\t\t: force nandcode, overwriting any code detected in the dol file\n");
}
void ShowDolInformation(std::unique_ptr<FileInfo>& input)
{
	dolHeader* header = (dolHeader*)&input->Data[0];
	if(header == NULL)
		throw "Invalid dol header read";

	printf("input: %s\n", input->GetFilename());
	printf("Entrypoint: 0x%08X\nBSS Address : 0x%08X\nBSS Size: 0x%08X\n\n", (unsigned int)ForceBigEndian(header->entrypoint) , (unsigned int)ForceBigEndian(header->addressBSS) , (unsigned int)ForceBigEndian(header->sizeBSS) );
	printf("Text Sections:\n");
	for(int i = 0;i < 6;i++)
	{
		if(header->sizeText[i] && header->addressText[i] && header->offsetText[i])
		{
			//valid info. swap and display
			printf("offset : 0x%08X\taddress : 0x%08X\tsize : 0x%08X\n", (unsigned int)ForceBigEndian(header->offsetText[i]), (unsigned int)ForceBigEndian(header->addressText[i]), (unsigned int)ForceBigEndian(header->sizeText[i]));
		}
	}

	printf("\nData Sections:\n");
	for(int i = 0;i <= 10;i++)
	{
		if(header->sizeData[i] && header->addressData[i] && header->offsetData[i])
		{
			//valid info. swap and display
			printf("offset : 0x%08X\taddress : 0x%08X\tsize : 0x%08X\n", (unsigned int)ForceBigEndian(header->offsetData[i]), (unsigned int)ForceBigEndian(header->addressData[i]), (unsigned int)ForceBigEndian(header->sizeData[i]));
		}
	}
}
int main(int argc, char **argv)
{
	printf("DacoTaco's OpenDolboot : Version %s\n\n",VERSION);
	if(argc < 3)
	{
		ShowHelp();
		return 0;
	}

	try
	{
		std::string inputFile = "";
		std::string outputFile = "";
		std::string nandCodeFile = "";
		std::vector<std::string> argumentList;
		bool showInfo = false;
		bool overwriteNandLoader = false;
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
				else if(argument == "-f")
					overwriteNandLoader = true;
				else if(argument == "-h") // -h / help
				{
					ShowHelp();
					return 0;
				}
				else if(argument != "-n") //all unknown arguments
				{
					printf("unknown arg '%s'\n",argument.c_str());
					ShowHelp();
				}
			}
			else if(prev_arg == "-n")
			{
				if(nandCodeFile.size() > 0)
					ShowHelp();

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
				ShowHelp();
				return 0;
			}	

			prev_arg = argument;
		}

		if(inputFile.size() == 0 || (showInfo == false && outputFile.size() == 0))
		{
			ShowHelp();
			return 0;	
		}
		
		//get input file info
		auto input = std::make_unique<FileInfo>(inputFile);
		if(showInfo)
		{
			ShowDolInformation(input);
			return 0;
		}

		//if the overwrite flag was set, we will attempt to remove the nandloader of the input
		//this will make it possible to always inject our own code
		auto nandLoaderInjector = std::make_unique<NandLoaderInjector>();
		if(overwriteNandLoader)
			nandLoaderInjector->RemoveNandLoader(input);

		//set & allocate new file data
		auto output = std::make_unique<FileInfo>(outputFile, false);	
		if(nandCodeFile.size() == 0)
		{
			nandLoaderInjector->InjectNandLoader(input, output);
		}
		else
		{
			auto nandLoader = std::make_unique<FileInfo>(nandCodeFile);
			nandLoaderInjector->InjectNandLoader(input, nandLoader, output);
		}

		output->WriteFile();
		printf("Done! check %s to verify!\n", output->GetFilename());
		return 0;
	}
	catch (const std::string& ex)
	{
		printf(ex.c_str());
	}
	catch (char const* ex)
	{
		printf(ex);
	}
	catch (...)
	{
		printf("exception was thrown, please report this.");
	}

	return 1;
}
