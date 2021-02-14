/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019  crediar

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
#include <gctypes.h>
#include "version.h"

unsigned char same_version(version_t v1, version_t v2)
{
	return v1.major == v2.major &&
		v1.minor == v2.minor &&
		v1.patch == v2.patch;
}

unsigned char smaller_version(version_t v1, version_t v2)
{
	return v1.major < v2.major ||
		v1.minor < v2.minor ||
		v1.patch < v2.patch;
}