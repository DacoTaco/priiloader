/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019 DacoTaco

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
#ifndef _VERSION_H_
#define _VERSION_H_

//typedefs

typedef struct _version_t
{
	unsigned char major;
	unsigned char minor;
	unsigned char patch;
	unsigned char sub_version;
} version_t;

typedef struct {
	version_t prod_version;
	unsigned int prod_sha1_hash[5];
	version_t beta_version;
	unsigned int beta_sha1_hash[5];
} UpdateStruct;

//defines
#define VERSION_MAJOR	0
#define VERSION_MINOR	10
#define VERSION_PATCH	0
#define VERSION_BETA	0
#define VERSION (version_t){ VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_BETA }
#define VERSION_MERGED (unsigned int)((VERSION_MAJOR << 16) + (VERSION_MINOR << 8) + (VERSION_PATCH))

unsigned char same_version(version_t v1, version_t v2);
unsigned char smaller_version(version_t v1, version_t v2);

#endif