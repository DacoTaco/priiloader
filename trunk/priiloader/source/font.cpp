/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

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

unsigned long trans_font(unsigned long int value, int add_color[5])
{
	char val[11], t[3], valou[11];
	unsigned long valout;
	int new_color[5];
	int i;
	sprintf(val, "%08lX", value);
	val[10]='\0';
	for(i=0;i<4;i++)
	{
		strncpy(t, val+(i*2), 2);
		t[2]='\0';
		new_color[i] = strtol(t,NULL,16);
	}
	for(i=0;i<4;i++)
	{
		if(new_color[i] + add_color[i] > 255)
			new_color[i] = 255;
		else if(new_color[i] + add_color[i] < 0)
			new_color[i] = 0;
		else
			new_color[i] = new_color[i] + add_color[i];
	}

	sprintf(valou, "0x%02X%02X%02X%02X", new_color[0], new_color[1], new_color[2], new_color[3]);
	valout = strtoul(valou, NULL, 16);
	return valout;
}

void PrintCharY( int xx, int yy, char c )
{
	//selected text
	unsigned long* fb = (unsigned long*)VIDEO_GetCurrentFramebuffer();

	int change_color[5];
	change_color[0] =  10;
	change_color[1] =  30;
	change_color[2] =  50;
	change_color[3] = -10;

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
	char *astr = (char*)malloc( 2048 );
	memset( astr, 0, 2048 );

	va_list ap;
	va_start( ap, str );

	vsprintf( astr, str, ap);

	va_end( ap );

	PrintString( col, x, y, astr );

	free(astr);
}
