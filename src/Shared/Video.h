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

// upstream these macros into libogc imo to complement VI_TVMODE (https://libogc.devkitpro.org/video__types_8h.html)
#define VI_TVMODE_FMT(viTVMode)   (viTVMode >> 2)    // = VI_NTSC / VI_PAL / VI_MPAL / VI_DEBUG / VI_DEBUG_PAL / VI_EURGB60
#define VI_TVMODE_MODE(viTVMode)  (viTVMode & 0b11)  // = VI_INTERLACE / VI_NON_INTERLACE / VI_PROGRESSIVE

#define TEXT_OFFSET(X) ((((rmode->viWidth) / 2 ) - (strnlen((X), 128)*13/2))>>1)

void InitVideo(void);
void InitVideoWithConsole(void);
void ShutdownVideo(void);
void ConfigureVideoMode(GXRModeObj* videoMode);

#endif