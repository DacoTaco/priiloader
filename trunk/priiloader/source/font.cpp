/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2013  crediar

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
#include <vector>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>

#include "settings.h"
#include "font.h"
#include "mem2_manager.h"

u32 trans_font(u32 value, int add_color[4])
{
	if (!add_color) 
		gprintf("SOMEONE DONE FUCKED UP!!!\n");
	u32 i, out = 0;
	/* in a,b,g,r order */
	for (i = 0; i < 4; i++)
	{
		u32 color = (value >> (8*i)) & 0xff;
		color += add_color[3-i];
		if (color & 0x80000000) 
			color = 0;
		else if (color > 0xff) 
			color = 0xff;
		out |= (color << (8*i));
	}
	return out;
}

void PrintCharY( int xx, int yy, char c )
{
	//selected text
	unsigned long* fb = (unsigned long*)VIDEO_GetCurrentFramebuffer();

	int change_color[4] = {10,30,50,-10};

	if( fb == NULL )
		return;

	if( c >= 0x7E || c < 0x20)
		c = ' ';
	for( int x=1; x <7; ++x)
	{
		for( int y=0; y<16; ++y)
		{
			if( SGetSetting(SETTING_BLACKBACKGROUND))
			{
				if (wii_font_Bitmap[x+(y+(c-' ')*16)*8] == 0xFF80FF80)
					fb[(x+xx)+(y+yy)*320] = 0x00800080;
				else
				{
					fb[(x+xx)+(y+yy)*320] = 0xFFFFFFFF - trans_font(wii_font_Bitmap[x+(y+(c-' ')*16)*8], change_color); //+ 0x000B0FFD;
				}
			}
			else
			{
				if( wii_font_Bitmap[x+(y+(c-' ')*16)*8] != 0xFF80FF80)
					fb[(x+xx)+(y+yy)*320] = trans_font(wii_font_Bitmap[x+(y+(c-' ')*16)*8], change_color);
				else
					fb[(x+xx)+(y+yy)*320] = wii_font_Bitmap[x+(y+(c-' ')*16)*8];
			}
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
			{
				if (wii_font_Bitmap[x+(y+(c-' ')*16)*8] == 0xFF80FF80)
					fb[(x+xx)+(y+yy)*320] = 0x00800080;
				else
					fb[(x+xx)+(y+yy)*320] = 0xFFFFFFFF - wii_font_Bitmap[x+(y+(c-' ')*16)*8];
			}
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
	char astr[2048];
	memset( astr, 0, 2048 );

	va_list ap;
	va_start( ap, str );

	vsnprintf(astr, 2047, str, ap);

	va_end( ap );

	PrintString( col, x, y, astr );
}
