/*
priiloader - A tool which allows to change the default boot up sequence on the Wii console

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

#ifndef __GECKO_H___
#define __GECKO_H___

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef DEBUG
#define gdprintf gprintf
#else
#define gdprintf(...)
#endif

bool IsUsbGeckoDetected();
void InitGDBDebug(void);
void SetDumpDebug(u8 value);
void CheckForGecko( void );
void gprintf(const char* str, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

