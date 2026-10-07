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

#include <algorithm>

#include "../include/DolBinary.hpp"

DolBinary::DolBinary(const std::string &filename) : FileInfo(filename, true)
{
	if (Data.size() < sizeof(dolHeader))
		throw "Invalid dol: file is too small to contain a valid dol header";

	TextSections = std::vector<DOLSection>();
	DataSections = std::vector<DOLSection>();
	const dolHeader *header = reinterpret_cast<const dolHeader *>(Data.data());

	for (unsigned char i = 0; i < MAX_TEXT_SECTIONS; ++i)
	{
		DOLSection section = {
			.Address = BigEndianToHost(header->addressText[i]),
			.Size = BigEndianToHost(header->sizeText[i]),
			.Offset = BigEndianToHost(header->offsetText[i]),
			.Index = i,
		};

		if (section.Offset == 0 || section.Address == 0)
			continue;

		section.Data = Data.data() + section.Offset;
		TextSections.push_back(section);
	}

	for (unsigned char i = 0; i < MAX_DATA_SECTIONS; ++i)
	{
		DOLSection section = {
			.Address = BigEndianToHost(header->addressData[i]),
			.Size = BigEndianToHost(header->sizeData[i]),
			.Offset = BigEndianToHost(header->offsetData[i]),
			.Index = i,
		};

		if (section.Offset == 0 || section.Address == 0)
			continue;

		section.Data = Data.data() + section.Offset;
		DataSections.push_back(section);
	}
}

unsigned int DolBinary::GetEntryPoint()
{
	const dolHeader *header = reinterpret_cast<const dolHeader *>(Data.data());
	return BigEndianToHost(header->entrypoint);
}

unsigned int DolBinary::GetBSSAddress()
{
	const dolHeader *header = reinterpret_cast<const dolHeader *>(Data.data());
	return BigEndianToHost(header->addressBSS);
}

unsigned int DolBinary::GetBSSSize()
{
	const dolHeader *header = reinterpret_cast<const dolHeader *>(Data.data());
	return BigEndianToHost(header->sizeBSS);
}

void DolBinary::Validate(bool updateValidation)
{
	int totalSize = 0;
	auto messages = std::vector<Message>();
	const auto minorMessageLevel = updateValidation ? MessageLevel::Error : MessageLevel::Warning;
	const auto majorMessageLevel = updateValidation ? MessageLevel::Critical : MessageLevel::Error;
	const auto hasCriticalErrors = [&messages, &majorMessageLevel]()
	{
		return std::any_of(messages.begin(), messages.end(),
						   [&majorMessageLevel](const Message &msg)
						   {
							   return msg.Level >= majorMessageLevel;
						   });
	};

	if (!IsAligned())
		messages.push_back({MessageLevel::Warning, "input file is not 32 byte aligned, this CAN cause issues with booting!"});

	for (auto &section : TextSections)
	{
		if (hasCriticalErrors())
			break;

		auto index = std::to_string(section.Index);
		if (section.Offset > Data.size() || section.Offset + section.Size > Data.size())
		{
			messages.push_back({majorMessageLevel, "Invalid dol: text section #" + index + " is out of bounds"});
			break;
		}

		if (!(updateValidation && section.IsNandLoaderSection()) && !section.IsAligned())
			messages.push_back({minorMessageLevel, "Text section #" + index + " has an unaligned address/size, this will cause issues with booting!"});

		totalSize += section.Size;
		if (totalSize > Data.size())
		{
			messages.push_back({majorMessageLevel, "Invalid dol: text section #" + index + " exceeds file size"});
			break;
		}
	}

	for (auto &section : DataSections)
	{
		if (hasCriticalErrors())
			break;

		auto index = std::to_string(section.Index);
		if (section.Offset > Data.size() || section.Offset + section.Size > Data.size())
		{
			messages.push_back({majorMessageLevel, "Invalid dol: data section #" + index + " is out of bounds"});
			break;
		}

		if (!(updateValidation && section.IsNandLoaderSection()) && !section.IsAligned())
			messages.push_back({minorMessageLevel, "Data section #" + index + " has an unaligned address/size, this will cause issues with booting!"});

		totalSize += section.Size;
		if (totalSize > Data.size())
		{
			messages.push_back({majorMessageLevel, "Invalid dol: data section #" + index + " exceeds file size"});
			break;
		}
	}

	// nice and clean
	if (messages.size() == 0)
		return;

	// or errors...
	// all messages < the minor level = just printed
	// all messages >= criticalerrors = thrown
	auto hasErrors = hasCriticalErrors();
	std::string message = "";
	for (auto &msg : messages)
	{
		if (msg.Level < minorMessageLevel)
			printf("%s\n", msg.Combine().c_str());
		else
			message += msg.Combine() + "\n";
	}

	// only less than minor messages
	if (message.size() == 0)
		return;

	if (hasErrors || updateValidation)
		throw message;
	else
		printf("%s", message.c_str());
}