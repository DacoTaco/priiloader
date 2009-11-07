/*

preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2009  crediar

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

#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <vector>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>

#include "settings.h"
#include "font.h"

void PrintCharY( int xx, int yy, char c )
{
	unsigned long* fb = (unsigned long*)VIDEO_GetCurrentFramebuffer();

	if( fb == NULL )
		return;

	if( c >= 0x7F || c < 0x20)
		c = ' ';

	for( int x=1; x <7; ++x)
	{
		for( int y=0; y<16; ++y)
		{
			if( SGetSetting(SETTING_BLACKBACKGROUND))
				fb[(x+xx)+(y+yy)*320] = wii_font_r_Bitmap_black[x+(y+(c-' ')*16)*8];
			else
				fb[(x+xx)+(y+yy)*320] = wii_font_r_Bitmap[x+(y+(c-' ')*16)*8];
		}
	}
}
void PrintCharW( int xx, int yy, char c )
{
	unsigned long* fb = (unsigned long*)VIDEO_GetCurrentFramebuffer();

	if( fb == NULL )
		return;

	if( c >= 0x7F || c < 0x20)
		c = ' ';

	for( int x=1; x <7; ++x)
	{
		for( int y=0; y<16; ++y)
		{
			if( SGetSetting(SETTING_BLACKBACKGROUND))
				fb[(x+xx)+(y+yy)*320] = wii_font_Bitmap_black[x+(y+(c-' ')*16)*8];
			else
				fb[(x+xx)+(y+yy)*320] = wii_font_Bitmap[x+(y+(c-' ')*16)*8];
		}
	}
}

void PrintString( int col, int x, int y, char *str )
{
	int i=0;
	while(str[i]!='\0')
	{
		if( col )
			PrintCharY( x+i*6, y, str[i] );
		else
			PrintCharW( x+i*6, y, str[i] );
		i++;
	}
}
void PrintFormat(int col, int x, int y, const char *str, ... )
{
	char *astr = (char*)malloc( 2048 );
	memset( astr, 0, 2048 );

	va_list ap;
	va_start( ap, str );

	vsprintf( astr, str, ap);

	va_end( ap );

	PrintString( col, x, y, astr );

	free(astr);
}
