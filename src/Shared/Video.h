/*
Videop.cpp - Wii Video functionality

Copyright (C) 2023
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
#ifndef _VIDEO_H_
#define _VIDEO_H_

#include <gctypes.h>
#include <ogc/gx_struct.h>

extern GXRModeObj *rmode;
extern void *xfb;

#define TEXT_OFFSET(X) ((((rmode->viWidth) / 2 ) - (strnlen((X), 128)*13/2))>>1)

void InitVideo(void);
void InitVideoWithConsole(void);
void ShutdownVideo(void);
void ConfigureVideoMode(GXRModeObj* videoMode);

#endif