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
#include <string.h>
#include <vector>
#include <unistd.h>

#include "gecko.h"
#include "hacks.h"
#include "settings.h"
#include "error.h"
#include "font.h"
#include "mem2_manager.h"

std::vector<hack> hacks;
std::vector<hack_hash> hacks_hash;
u32 *states=NULL;
u32 *states_hash=NULL;
extern u8 error;

unsigned int foff=0;
char *GetLine( char *astr, unsigned int len)
{
	if( foff >= len )
	{
		return NULL;
	}

	unsigned int llen = 0;
	if( strstr(	astr+foff, "\n" ) == NULL )
	{
		if( strstr( astr+foff, "\0" ) == NULL )
		{
			return 0;
		}
		if(strstr( astr+foff, "\0" ) == (astr+foff))
		{
			llen = len - foff;
		}
		else
			llen = strstr( astr+foff, "\0" ) - (astr+foff);
		if( llen == 0 )
		{
			return NULL;
		}
	}
	else
	{
		llen = strstr( astr+foff , "\n" ) - (astr+foff);
		if( llen == 0 )
		{
			return NULL;
		}
	}

	//printf("%d\n", llen );

	char *lbuf = (char*)mem_malloc( llen );
	if( lbuf == NULL )
	{
		error = ERROR_MALLOC;
		return 0;
	}
	memset(lbuf,0,llen);
	//silly fix for linux newlines...
	if(strstr( astr, "\r\n" ) != NULL )
	{
		//must be a windows newline D:<
		memcpy( lbuf, astr+foff, llen -1);
		lbuf[llen-1] = 0;
	}
	else
	{
		//linux it is
		memcpy( lbuf, astr+foff, llen);
		lbuf[llen] = 0;
	}
	foff+=llen+1;
	return lbuf;
}
u32 LoadHacks( bool Force_Load_Nand )
{
	if( hacks.size() ) //Hacks already loaded
	{
		hacks.clear();
		if(states)
		{
			mem_free(states);
		}
	}
	if(foff > 0)
		foff=0;
	bool mode = true;
	s32 fd=0;
	char *buf=NULL;
	fstats *status = NULL;
	unsigned int size=0;
	FILE* in = NULL;
	if(!Force_Load_Nand)
	{
		in = fopen ("fat:/hacks.ini","rb");
	}
	if( !in )
	{
		fd = ISFS_Open("/title/00000001/00000002/data/hacks.ini", 1 );
		if( fd < 0 )
		{
			gprintf("LoadHacks : Hacks.ini not on FAT or Nand. ISFS_Open error %d\n",fd);
			return 0;
		} 
		mode = false;
	}
	if( mode )	//read file from FAT
	{
		//read whole file in
		fseek( in, 0, SEEK_END );
		size = ftell(in);
		fseek( in, 0, 0);

		if( size == 0 )
		{
			PrintFormat( 1, ((640/2)-((strlen("Error \"hacks.ini\" is 0 byte!"))*13/2))>>1, 208, "Error \"hacks.ini\" is 0 byte!");
			sleep(5);
			return 0;
		}

		buf = (char*)mem_align( 32, (size+31)&(~31));
		if( buf == NULL )
		{
			error = ERROR_MALLOC;
			return 0;
		}
		memset( buf, 0, (size+31)&(~31) );
		if(fread( buf, sizeof( char ), size, in ) != size )
		{
			mem_free(buf);
			PrintFormat( 1, ((640/2)-((strlen("Error reading \"hacks.ini\""))*13/2))>>1, 208, "Error reading \"hacks.ini\"");
			sleep(5);
			return 0;
		}
		fclose(in);

	} else {	//read file from NAND

		status = (fstats *)mem_align( 32, (sizeof( fstats )+31)&(~31) );
		ISFS_GetFileStats( fd, status);
		size = status->file_length;
		mem_free(status);
		buf = (char*)mem_align( 32, (size+31)&(~31) );
		if( buf == NULL )
		{
			error = ERROR_MALLOC;
			return 0;
		}
		memset( buf, 0, (size+31)&(~31) );
		ISFS_Read( fd, buf, size );
		ISFS_Close( fd );
	}

	buf[size] = 0;

//read stuff into a nice struct
	char *str=buf;
	char *lbuf=NULL;
	unsigned int line = 1;

	lbuf = GetLine( str, size );
	if( lbuf == NULL )
	{
		//printf("Error 1: syntax error : expected EOF @ line %d\n", line );
		PrintFormat( 1, ((640/2)-((strlen("Syntax error : unexpected EOF @ line   "))*13/2))>>1, 208, "Syntax error : unexpected EOF @ line %d", line);
		mem_free(buf);
		hacks.clear();
		sleep(5);
		return 0;
	}

	while(1)
	{
		if( lbuf == NULL )
			break;

		//printf("\"%s\"\n", lbuf );

		if( strstr( lbuf, "[" ) == NULL )
		{
			//printf("Error 2: syntax error : missing '[' before '\\n' @ line %d\n", line );
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '[' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '[' before 'n' @ line %d", line);
			hacks.clear();
			sleep(5);
			return 0;
		}

		char *s = strtok( lbuf+(strstr( lbuf, "[" )-lbuf) + 1, "]");
		if( s == NULL )
		{
			//printf("Error 3: syntax error : missing ']' before '\\n' @ line %d\n", line );
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing ']' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing ']' before 'n' @ line %d", line);
			hacks.clear();
			sleep(5);
			return 0;
		}

		//printf("\"%s\"\n", s );

		hacks.resize( hacks.size() + 1 );
		hacks[hacks.size()-1].desc = new char[strlen(s)+1];
		memcpy( hacks[hacks.size()-1].desc, s, strlen(s)+1 );
		hacks[hacks.size()-1].desc[strlen(s)] = 0;
		mem_free(lbuf);
		line++;
		lbuf = GetLine( str, size );
		if( lbuf == NULL )
		{
			//printf("Error 4: syntax error : expected EOF @ line %d\n", line );
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : unexpected EOF @ line   "))*13/2))>>1, 208, "Syntax error : unexpected EOF @ line %d", line);
			hacks.clear();
			sleep(5);
			return 0;
		}

		if( strncmp(lbuf,"version",7) == 0 )
		{
			if( strstr( lbuf, "=" ) ==  NULL )
			{
				//printf("Error 5: syntax error : missing '=' before '\\n' @ line %d\n", line );
				PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '=' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '=' before 'n' @ line %d", line);
				hacks.clear();
				sleep(5);
				return 0;
			}
			s = strtok( lbuf+(strstr( lbuf, "=" )-lbuf) + 1, "\n");
			if( s == NULL )
			{
				//printf("Error 5: syntax error : missing value before '\\n' @ line %d\n", line );
				PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing value before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing value before 'n' @ line %d", line);
				hacks.clear();
				sleep(5);
				return 0;
			}
		} else {
			//printf("Error 6: syntax error : expected 'version' before '\\n' @ line %d\n", line );
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : expected 'version' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : expected 'version' before 'n' @ line %d", line);
			hacks.clear();
			sleep(5);
			return 0;
		}

		//printf("\"%s\"\n", s );

		hacks[hacks.size()-1].version = atoi( s );

		if( hacks[hacks.size()-1].version == 0 )
		{
			//printf("Error 12: syntax error : version is zero @ line %d\n", line );
			PrintFormat( 1, ((640/2)-((strlen("Error : version is zero @ line   "))*13/2))>>1, 208, "Error : version is zero @ line %d", line);
			hacks.clear();
			sleep(5);
			return 0;
		}
		mem_free(lbuf);

		while(1)
		{
			line++,
			lbuf = GetLine( str, size );
			if( lbuf == NULL )
				break;

			if( memcmp( lbuf, "offset", 6 ) == 0 )
			{
				//printf("offsets:");
				if( strstr( lbuf, "=" ) ==  NULL )
				{
					//printf("Error 5: syntax error : missing '=' before '\\n' @ line %d\n", line );
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '=' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '=' before 'n' @ line %d", line);
					hacks.clear();
					sleep(5);
					return 0;
				}
				s = strtok( lbuf+(strstr( lbuf, "=" )-lbuf) + 1, ",\n");
				if( s == NULL )
				{
					//printf("Error 7: syntax error : expected value before '\\n' @ line %d\n", line );
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : expected value before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : expected value before 'n' @ line %d", line);
					hacks.clear();
					sleep(5);
					return 0;
				}
				
				do{
					hacks[hacks.size()-1].offset.resize( hacks[hacks.size()-1].offset.size() + 1 );
					sscanf( s, "%x", &hacks[hacks.size()-1].offset[hacks[hacks.size()-1].offset.size()-1] );
					//printf("%08X", hacks[hacks.size()-1].offset[hacks[hacks.size()-1].offset.size()-1] );

				}while( (s = strtok( NULL,",\n")) != NULL);
				//printf("\n");

			} else if ( memcmp( lbuf, "value", 5 ) == 0 ) {

				//printf("value:");
				if( strstr( lbuf, "=" ) ==  NULL )
				{
					//printf("Error 5: syntax error : missing '=' before '\\n' @ line %d\n", line );
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '=' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '=' before 'n' @ line %d", line);
					hacks.clear();
					sleep(5);
					return 0;
				}
				s = strtok( lbuf+(strstr( lbuf, "=" )-lbuf) + 1, ",\n");
				if( s == NULL )
				{
					//printf("Error 8: syntax error : expected value before '\\n' @ line %d\n", line );
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : expected value before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : expected value before 'n' @ line %d", line);
					hacks.clear();
					sleep(5);
					return 0;
				}
				
				do{
					hacks[hacks.size()-1].value.resize( hacks[hacks.size()-1].value.size() + 1 );
					sscanf( s, "%x", &hacks[hacks.size()-1].value[hacks[hacks.size()-1].value.size()-1] );
					//printf("%08X", hacks[hacks.size()-1].value[hacks[hacks.size()-1].value.size()-1] );

				}while( (s = strtok( NULL,",\n")) != NULL);
				//printf("\n");

			} else if( hacks[hacks.size()-1].value.size() > 0 && hacks[hacks.size()-1].offset.size() > 0 ) {
				if( hacks[hacks.size()-1].value.size() > hacks[hacks.size()-1].offset.size() )
				{
					//printf("Error 9: syntax error : found more values than offsets @ line %d\n", line );
					PrintFormat( 1, ((640/2)-((strlen("Error : found more values than offsets @ line   "))*13/2))>>1, 208, "Error : found more values than offsets @ line %d", line);
					hacks.clear();
					sleep(5);
					return 0;
				} else if ( hacks[hacks.size()-1].value.size() < hacks[hacks.size()-1].offset.size() ) {

					//printf("Error 10: syntax error : found more offsets than values @ line %d\n", line );
					PrintFormat( 1, ((640/2)-((strlen("Error : found more offsets than values @ line   "))*13/2))>>1, 208, "Error : found more offsets than values @ line %d", line);
					hacks.clear();
					sleep(5);
					return 0;
				}
				break;
			} else {
				//printf("Error 11: syntax error : expected 'offset' or 'value' before '\\n' @ line %d\n", line );
				PrintFormat( 1, ((640/2)-((strlen("Syntax error : expected 'offset' or 'value' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : expected 'offset' or 'value' before 'n' @ line %d", line);
				hacks.clear();
				sleep(5);
				return 0;
			}
			mem_free(lbuf);
		}
	}
	mem_free(buf);

	if(hacks.size()==0)
		return 0;

	//load hack states (on/off)
	if ( states == NULL )
	{
		states = (u32*)mem_align( 32, ((sizeof( u32 ) * hacks.size())+31)&(~31) );
		if( states == NULL )
		{
			error = ERROR_MALLOC;
			return 0;
		}
	}
	memset( states, 0, ((sizeof( u32 ) * hacks.size())+31)&(~31) );

	fd = ISFS_Open("/title/00000001/00000002/data/hacks_s.ini", 1|2 );

	if( fd < 0 )
	{
		//file not found create a new one
		if(ISFS_CreateFile("/title/00000001/00000002/data/hacks_s.ini", 0, 3, 3, 3)<0)
			return 0;

		fd = ISFS_Open("/title/00000001/00000002/data/hacks_s.ini", 1|2 );
		if( fd < 0 )
			return 0;

		ISFS_Write( fd, states, sizeof( u32 ) * hacks.size() );

		ISFS_Seek( fd, 0, 0 );
	}

	status = (fstats *)mem_align( 32, (sizeof(fstats)+31)&(~31) );
	if( status == NULL )
	{
		error = ERROR_MALLOC;
		return 0;
	}
	memset( status, 0, (sizeof(fstats)+31)&(~31) );

	if(ISFS_GetFileStats( fd, status)<0)
	{
		mem_free(status);
		return 0;
	}

	u8 *fbuf = (u8 *)mem_align( 32, (status->file_length+31)&(~31) );
	if( fbuf == NULL )
	{
		mem_free(status);
		error = ERROR_MALLOC;
		return 0;
	}
	memset( fbuf, 0, (status->file_length+31)&(~31) );

	if(ISFS_Read( fd, fbuf, status->file_length )<0)
	{
		mem_free(status);
		mem_free(fbuf);
		return 0;
	}

	if( sizeof( u32 ) * hacks.size() < status->file_length )
		memcpy( states, fbuf, sizeof( u32 ) * hacks.size() );
	else
		memcpy( states, fbuf, status->file_length );

	mem_free(fbuf);
	mem_free(status);
	ISFS_Close( fd );

	//Set all hacks from other regions to 0
	unsigned int sysver = GetSysMenuVersion();
	for( u32 i=0; i < hacks.size(); ++i)
	{
		if( hacks[i].version != sysver )
		{
			states[i] = 0;
		}
	}
	return 1;
}
s8 LoadHacks_Hash( bool Force_Load_Nand )
{	
	if( hacks_hash.size() ) //hacks_hash already loaded
	{
		hacks_hash.clear();
		if(states_hash)
		{
			mem_free(states_hash);
		}
	}
	if(foff != 0)
		foff=0;
	bool mode = true;
	s32 fd=0;
	char *buf=NULL;
	fstats *status = NULL;
	unsigned int size=0;
	FILE* in = NULL;
	if(!Force_Load_Nand)
	{
		in = fopen ("fat:/hacks_hash.ini","rb");
	}
	if( !in )
	{
		fd = ISFS_Open("/title/00000001/00000002/data/hackshas.ini", 1 );
		if( fd < 0 )
		{
			gprintf("LoadHacks : hacks_hash.ini not on FAT or Nand. ISFS_Open error %d\n",fd);
			return 0;
		} 
		mode = false;
	}
	if( mode )	//read file from FAT
	{
		//read whole file in
		fseek( in, 0, SEEK_END );
		size = ftell(in);
		fseek( in, 0, 0);

		if( size == 0 )
		{
			PrintFormat( 1, ((640/2)-((strlen("Error \"hacks_hash.ini\" is 0 byte!"))*13/2))>>1, 208, "Error \"hacks_hash.ini\" is 0 byte!");
			sleep(5);
			return 0;
		}

		buf = (char*)mem_align( 32, (size+31)&(~31));
		if( buf == NULL )
		{
			error = ERROR_MALLOC;
			return 0;
		}
		memset( buf, 0, (size+31)&(~31) );
		if(fread( buf, sizeof( char ), size, in ) != size )
		{
			mem_free(buf);
			PrintFormat( 1, ((640/2)-((strlen("Error reading \"hacks_hash.ini\""))*13/2))>>1, 208, "Error reading \"hacks_hash.ini\"");
			sleep(5);
			return 0;
		}
		fclose(in);

	} else {	//read file from NAND

		status = (fstats *)mem_align( 32, (sizeof( fstats )+31)&(~31) );
		ISFS_GetFileStats( fd, status);
		size = status->file_length;
		mem_free(status);
		buf = (char*)mem_align( 32, (size+31)&(~31) );
		if( buf == NULL )
		{
			error = ERROR_MALLOC;
			return 0;
		}
		memset( buf, 0, (size+31)&(~31) );
		ISFS_Read( fd, buf, size );
		ISFS_Close( fd );
	}
	buf[size] = 0;

//read stuff into a nice struct
	char *str=buf;
	char *lbuf=NULL;
	unsigned int line = 1;
	patch_struct temp;
	temp.hash.clear();
	temp.patch.clear();
	hack_hash new_hacks_hash;

	lbuf = GetLine( str, size );
	if( lbuf == NULL )
	{
		PrintFormat( 1, ((640/2)-((strlen("Syntax error : unexpected EOF @ line   "))*13/2))>>1, 208, "Syntax error : unexpected EOF @ line %d", line);
		mem_free(buf);
		hacks_hash.clear();
		sleep(5);
		return 0;
	}

	while(1)
	{
		if( lbuf == NULL )
			break;

		if( strstr( lbuf, "[" ) == NULL )
		{
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '[' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '[' before 'n' @ line %d", line);
			hacks_hash.clear();
			sleep(5);
			return 0;
		}

		char *s = strtok( lbuf+(strstr( lbuf, "[" )-lbuf) + 1, "]");
		if( s == NULL )
		{
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing ']' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing ']' before 'n' @ line %d", line);
			hacks_hash.clear();
			sleep(5);
			return 0;
		}

		new_hacks_hash.desc = s;
		mem_free(lbuf);

		line++;
		lbuf = GetLine( str, size );
		if( lbuf == NULL )
		{
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : unexpected EOF @ line   "))*13/2))>>1, 208, "Syntax error : unexpected EOF @ line %d", line);
			hacks.clear();
			sleep(5);
			return 0;
		}

		if( strncmp( lbuf, "maxversion",10 ) == 0 )
		{
			if( strstr( lbuf, "=" ) ==  NULL )
			{
				PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '=' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '=' before 'n' @ line %d", line);
				hacks_hash.clear();
				sleep(5);
				return 0;
			}
			s = strtok( lbuf+(strstr( lbuf, "=" )-lbuf) + 1, "\n");
			if( s == NULL )
			{
				PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing value before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing value before 'n' @ line %d", line);
				hacks_hash.clear();
				sleep(5);
				return 0;
			}
		} else {
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : expected 'version' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : expected 'version' before 'n' @ line %d", line);
			hacks.clear();
			sleep(5);
			return 0;
		}

		new_hacks_hash.max_version = atoi( s );

		if( new_hacks_hash.max_version == 0 )
		{
			PrintFormat( 1, ((640/2)-((strlen("Error : maxversion is zero @ line   "))*13/2))>>1, 208, "Error : maxversion is zero @ line %d", line);
			hacks.clear();
			sleep(5);
			return 0;
		}

		mem_free(lbuf);
		line++;
		lbuf = GetLine( str, size );
		if( lbuf == NULL )
		{
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : unexpected EOF @ line   "))*13/2))>>1, 208, "Syntax error : unexpected EOF @ line %d", line);
			hacks.clear();
			sleep(5);
			return 0;
		}

		if( strncmp( lbuf, "minversion",10 ) == 0 )
		{
			if( strstr( lbuf, "=" ) ==  NULL )
			{
				PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '=' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '=' before 'n' @ line %d", line);
				hacks_hash.clear();
				sleep(5);
				return 0;
			}
			s = strtok( lbuf+(strstr( lbuf, "=" )-lbuf) + 1, "\n");
			if( s == NULL )
			{
				PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing value before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing value before 'n' @ line %d", line);
				hacks_hash.clear();
				sleep(5);
				return 0;
			}
		} else {
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : expected 'minversion' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : expected 'minversion' before 'n' @ line %d", line);
			hacks_hash.clear();
			sleep(5);
			return 0;
		}

		new_hacks_hash.min_version = atoi( s );

		if( new_hacks_hash.min_version == 0 )
		{
			PrintFormat( 1, ((640/2)-((strlen("Error : minversion is zero @ line   "))*13/2))>>1, 208, "Error : minversion is zero @ line %d", line);
			hacks_hash.clear();
			sleep(5);
			return 0;
		}

		mem_free(lbuf);
		line++;
		lbuf = GetLine( str, size );
		if( lbuf == NULL )
		{
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : unexpected EOF @ line   "))*13/2))>>1, 208, "Syntax error : expected EOF @ line %d", line);
			hacks_hash.clear();
			sleep(5);
			return 0;
		}

		if( strncmp( lbuf, "amount",6 ) == 0 )
		{
			if( strstr( lbuf, "=" ) ==  NULL )
			{
				PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '=' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '=' before 'n' @ line %d", line);
				hacks_hash.clear();
				sleep(5);
				return 0;
			}
			s = strtok( lbuf+(strstr( lbuf, "=" )-lbuf) + 1, "\n");
			if( s == NULL )
			{
				PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing value before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing value before 'n' @ line %d", line);
				hacks_hash.clear();
				sleep(5);
				return 0;
			}
		} else {
			PrintFormat( 1, ((640/2)-((strlen("Syntax error : expected 'amount' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : expected 'patches' before 'n' @ line %d", line);
			hacks_hash.clear();
			sleep(5);
			return 0;
		}
		new_hacks_hash.amount = atoi( s );

		if( new_hacks_hash.amount == 0 )
		{
			PrintFormat( 1, ((640/2)-((strlen("Error : patches amount is zero @ line   "))*13/2))>>1, 208, "Error : patches amount is zero @ line %d", line);
			hacks_hash.clear();
			sleep(5);
			return 0;
		}
		mem_free(lbuf);
		while(1)
		{
			line++;
			lbuf = GetLine( str, size );
			if(temp.hash.size() > 0 && temp.patch.size() > 0)
			{
				new_hacks_hash.patches.push_back(temp);
				temp.hash.clear();
				temp.patch.clear();
			} 
			if( lbuf == NULL || new_hacks_hash.patches.size() == new_hacks_hash.amount )
			{
				if(	new_hacks_hash.patches.size() != new_hacks_hash.amount)
				{
					if( lbuf == NULL )
					{
						PrintFormat( 1, ((640/2)-((strlen("Syntax error : unexpected EOF @ line   "))*13/2))>>1, 208, "Syntax error : unexpected EOF @ line %d", line);
						temp.hash.clear();
						temp.patch.clear();
						hacks_hash.clear();
						sleep(5);
						return 0;
					}
				}
				break;
			}
			
			if( memcmp( lbuf, "hash", 4 ) == 0 )
			{
				if ( temp.hash.size() )
				{
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing 'patch' before line   "))*13/2))>>1, 208, "Syntax error : missing 'patch' before line %d", line);
					hacks_hash.clear();
					sleep(5);
					return 0;
				}
				if( strstr( lbuf, "=" ) ==  NULL )
				{
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '=' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '=' before 'n' @ line %d", line);
					hacks_hash.clear();
					sleep(5);
					return 0;
				}
				s = strtok( lbuf+(strstr( lbuf, "=" )-lbuf) + 1, ",\n");
				if( s == NULL )
				{
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : expected value before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : expected value before 'n' @ line %d", line);
					hacks_hash.clear();
					sleep(5);
					return 0;
				}
				
				do{
					unsigned int hash;
					uint8_t size = 0;
					sscanf(s,"%x",&hash);
					if((hash >> 24))
						size = 4;
					else if((hash & 0xFF0000) >> 16)
						size = 3;
					else if((hash & 0xFF00) >> 8)
						size = 2;
					else if(hash & 0xFF)
						size = 1;
					else
						size = 0;
					switch(size)
					{
						case 4: //full u32 like 0x90FFAADD
							temp.hash.push_back(hash >> 24); //first byte of u32 0x90
						case 3:
							temp.hash.push_back((hash & 0xFF0000) >> 16); //second byte of u32 0xFF
						case 2:
							temp.hash.push_back((hash & 0xFF00) >> 8); //3th byte of u32 0xAA
						case 1:
							temp.hash.push_back(hash & 0xFF); // 4th byte of u32 0xDD
						default:
							break;
					}
				
				}while( (s = strtok( NULL,",\n")) != NULL);
				
			} else if ( memcmp( lbuf, "patch", 5 ) == 0 ) 
			{
				if (temp.patch.size() || temp.hash.size() == 0 )
				{
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing 'hash' before line   "))*13/2))>>1, 208, "Syntax error : missing 'hash' before line %d", line);
					hacks_hash.clear();
					sleep(5);
					return 0;
				}
				if( strstr( lbuf, "=" ) ==  NULL )
				{
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : missing '=' before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : missing '=' before 'n' @ line %d", line);
					hacks_hash.clear();
					sleep(5);
					return 0;
				}
				s = strtok( lbuf+(strstr( lbuf, "=" )-lbuf) + 1, ",\n");
				if( s == NULL )
				{
					PrintFormat( 1, ((640/2)-((strlen("Syntax error : expected value before 'n' @ line   "))*13/2))>>1, 208, "Syntax error : expected value before 'n' @ line %d", line);
					hacks_hash.clear();
					sleep(5);
					return 0;
				}
				do{
					unsigned int patch;
					uint8_t size = 0;
					sscanf(s,"%x",&patch);
					if((patch >> 24))
						size = 4;
					else if((patch & 0xFF0000) >> 16)
						size = 3;
					else if((patch & 0xFF00) >> 8)
						size = 2;
					else if(patch & 0xFF)
						size = 1;
					else
						size = 0;
					switch(size)
					{
						case 4: // full u32 -> 4 bytes
							temp.patch.push_back(patch >> 24); //first byte of u32 -> 0x90
						case 3: // 24 bit, 3 bytes
							temp.patch.push_back((patch & 0xFF0000) >> 16); //second byte of u32 -> 0xFF
						case 2: // u16, 2 bytes
							temp.patch.push_back((patch & 0xFF00) >> 8); //3th byte of u32 -> 0xAA
						case 1: // u8, 1 byte
							temp.patch.push_back(patch & 0xFF); // 4th byte of u32 -> 0xDD
						default:
							break;
					}

				}while( (s = strtok( NULL,",\n")) != NULL);
			} else {
				if(new_hacks_hash.patches.size() > 0 || temp.hash.size() > 0 || temp.patch.size() > 0)
					PrintFormat( 1, ((640/2)-((strlen("Syntax Er: not enough 'hash' or 'patch' before line   "))*13/2))>>1, 208, "Syntax Er: not enough 'hash' or 'patch' before line %d", line);
				else
					PrintFormat( 1, ((640/2)-((strlen("Syntax Err : expected 'hash' or 'patch' before line   "))*13/2))>>1, 208, "Syntax Err : expected 'hash' or 'patch' before line %d", line);
				hacks_hash.clear();
				sleep(5);
				return 0;
			}
			mem_free(lbuf);
		}
		hacks_hash.push_back(new_hacks_hash);
		new_hacks_hash.patches.clear();
		new_hacks_hash.desc.clear();
		new_hacks_hash.amount = 0;
		new_hacks_hash.max_version = 0;
		new_hacks_hash.min_version = 0;
	}
	mem_free(buf);

	if(hacks_hash.size()==0)
		return 0;

	//load hack states_hash (on/off)
	if ( states_hash == NULL )
	{
		states_hash = (u32*)mem_align( 32, ((sizeof( u32 ) * hacks_hash.size())+31)&(~31) );
		if( states_hash == NULL )
		{
			error = ERROR_MALLOC;
			return 0;
		}
	}

	memset( states_hash, 0, ((sizeof( u32 ) * hacks_hash.size())+31)&(~31) );

	fd = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2 );

	if( fd < 0 )
	{
		//file not found create a new one
		fd = ISFS_CreateFile("/title/00000001/00000002/data/hacksh_s.ini", 0, 3, 3, 3);
		if(fd < 0 )
		{
			return 0;
		}

		fd = ISFS_Open("/title/00000001/00000002/data/hacksh_s.ini", 1|2 );
		if( fd < 0 )
			return 0;

		ISFS_Write( fd, states_hash, sizeof( u32 ) * hacks_hash.size() );

		ISFS_Seek( fd, 0, 0 );
	}

	status = (fstats *)mem_align( 32, (sizeof(fstats)+31)&(~31) );
	if( status == NULL )
	{
		error = ERROR_MALLOC;
		return 0;
	}

	memset( status, 0, (sizeof(fstats)+31)&(~31) );

	if(ISFS_GetFileStats( fd, status)<0)
	{
		mem_free(status);
		return 0;
	}

	u8 *fbuf = (u8 *)mem_align( 32, (status->file_length+31)&(~31) );
	if( fbuf == NULL )
	{
		mem_free(status);
		error = ERROR_MALLOC;
		return 0;
	}

	memset( fbuf, 0, (status->file_length+31)&(~31) );

	if(ISFS_Read( fd, fbuf, status->file_length )<0)
	{
		mem_free(status);
		mem_free(fbuf);
		return 0;
	}

	if( sizeof( u32 ) * hacks_hash.size() < status->file_length )
		memcpy( states_hash, fbuf, sizeof( u32 ) * hacks_hash.size() );
	else
		memcpy( states_hash, fbuf, status->file_length );

	mem_free(fbuf);
	mem_free(status);
	ISFS_Close( fd );

	//Set all hacks_hash for system menu > max_version || sysver < min_version to 0
	unsigned int sysver = GetSysMenuVersion();
	for( u32 i=0; i < hacks_hash.size(); ++i)
	{
		if( hacks_hash[i].max_version < sysver || hacks_hash[i].min_version > sysver)
		{
			states_hash[i] = 0;
		}
	}
	return 1;
}
