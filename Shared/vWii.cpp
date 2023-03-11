/*
vWii.cpp - vWii compatibility utils for priiloader

Copyright (C) 2022
GaryOderNichts
DacoTaco

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

#include <cstring>

#include "vWii.h"
#include "gecko.h"

#define LT_CHIPREVID (*(vu32*) 0xcd8005a0)

bool CheckvWii(void)
{
	// Check LT_CHIPREVID for cafe magic
	return (LT_CHIPREVID & 0xffff0000) == 0xcafe0000;
}