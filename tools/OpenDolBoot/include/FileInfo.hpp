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
#include <string>
#include <string.h>
#include <unistd.h>

class FileInfo 
{	
		std::string FileName;
	public:
		explicit FileInfo(const std::string& filename) : FileInfo(filename, true){}
		explicit FileInfo(const std::string& filename, bool readData);
		explicit FileInfo(const std::string& filename, const unsigned char* data, const unsigned int size);
		~FileInfo(){}
		std::vector<unsigned char> Data;
		const char* GetFilename();
		unsigned int GetFileSize();
		void WriteFile();
};
