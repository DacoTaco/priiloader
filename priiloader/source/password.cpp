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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "font.h"
#include "gecko.h"
#include "error.h"
#include "settings.h"
#include "mem2_manager.h"
#include "Input.h"

extern void ClearScreen();
extern bool PollDevices();

bool check_pass( char* pass )
{
	char letter[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	char lettera[] = "abcdefghijklmnopqrstuvwxyz0123456789";
	int len = strlen(pass);
	int i = 1;
	char pass2[len];
	sprintf(pass2, "%s", pass);
	char * pch;
	pch = strpbrk(letter, pass);
	while (pch != NULL)
	{
		i++;
		pch = strpbrk (pch+1,pass);
	}
	pch = strpbrk(lettera, pass2);
	while (pch != NULL)
	{
		i++;
		pch = strpbrk (pch+1,pass2);
	}
	if( i == len )
		return true;
	else
		return false;
}

void InstallPassword( void )
{
	char filepath[MAXPATHLEN];
	char *pbuf=NULL;
	unsigned int psize=0;
	char *ptr;
	char* password = NULL;
	char* file = NULL;
	char tfile[200];
	int redraw = 1;
	FILE* in = NULL;
	if (!PollDevices() )
	{
		PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to mount fat device!"))*13/2))>>1, 208, "Failed to mount fat device!");
		sleep(5);
		return;
	}

	while(1)
	{
		Input_ScanPads();
		u32 pressed = Input_ButtonsDown();

		if( redraw )
		{
			password = NULL;
			file = NULL;
			s32 pfd=0;
			pfd = ISFS_Open("/title/00000001/00000002/data/password.txt", 1 );
			if( pfd < 0 )
			{
				gprintf("InstallPassword: ISFS_Open(password.txt) failure. error %d\r\n",pfd);
			}
			else
			{
				fstats *pstatus = (fstats *)mem_align( 32, sizeof( fstats ) );
				ISFS_GetFileStats( pfd, pstatus);
				psize = pstatus->file_length;
				mem_free( pstatus );
				mem_free( pbuf );
				pbuf = (char*)mem_align( 32, ALIGN32(psize) );
				if( pbuf == NULL )
				{
					error = ERROR_MALLOC;
					return;
				}
				memset( pbuf, 0, psize );
				ISFS_Read( pfd, pbuf, psize );
				ISFS_Close( pfd );

				pbuf[psize] = 0;
				char se[5];
				if( strpbrk(pbuf , "\r\n") )
					sprintf(se, "\r\n");
				else
					sprintf(se, "\n");

				ptr = strtok(pbuf, se);
				while (ptr != NULL)
				{
					if(file == NULL && password != NULL)
						file = ptr;
					if(password == NULL)
						password = ptr;
					ptr = strtok (NULL, se);
					if(file != NULL && password != NULL)
						break;
				}
			}

			if(password == NULL)
				PrintFormat( 0, 16, 32, "Password:");
			else
			{
				PrintFormat( 0, 16, 32, "Password: %s", password);
				if( check_pass( password ) )
					PrintFormat( 1, (rmode->viWidth-48)>>1, 32, "OK");
				else
					PrintFormat( 0, (rmode->viWidth-48)>>1, 32, "--");
			}

			if(file == NULL)
			{
				PrintFormat( 0, 16, 48, "File:     wii_secure.dol");
				in = fopen("fat:/wii_secure.dol", "rb");
			}
			else
			{
				PrintFormat( 0, 16, 48, "File:     %s", file);
				sprintf(tfile, "fat:/%s", file);
				in = fopen(tfile, "rb");
			}
			if( in )
				PrintFormat( 1, (rmode->viWidth-48)>>1, 48, "OK");
			else
				PrintFormat( 0, (rmode->viWidth-48)>>1, 48, "--");
			fclose(in);

			PrintFormat( 0, ((rmode->viWidth /2)-((strlen("A(A) Install file"))*13/2))>>1, rmode->viHeight-64, "A(A) Install File");
			PrintFormat( 0, ((rmode->viWidth /2)-((strlen("2(X) Delete File "))*13/2))>>1, rmode->viHeight-48, "2(X) Delete File");
			redraw = 0;
		}

#ifdef DEBUG
		if ( pressed & INPUT_BUTTON_START )
			exit(0);
#endif
		if ( pressed & INPUT_BUTTON_B )
		{
			if(pbuf)
				mem_free(pbuf);
			break;
		}

		if ( pressed & INPUT_BUTTON_A )
		{
			ClearScreen();
	//Install file
			sprintf(filepath, "fat:/password.txt");
			FILE *passtxt = fopen(filepath, "rb" );
			if( passtxt == NULL )
			{
				PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Couldn't open \"password.txt\" for reading!"))*13/2))>>1, 208, "Couldn't open \"password.txt\" for reading!");
				sleep(5);
				return;
			}
			PrintFormat( 0, ((rmode->viWidth /2)-((strlen("Install \"password.txt\"..."))*13/2))>>1, 208, "Install \"password.txt\"...");

			//get size
			fseek( passtxt, 0, SEEK_END );
			unsigned int size = ftell( passtxt );
			fseek( passtxt, 0, 0 );

			char *buf = (char*)mem_align( 32, sizeof( char ) * size );
			memset( buf, 0, sizeof( char ) * size );

			fread( buf, sizeof( char ), size, passtxt );
			fclose( passtxt );

			//Check if there is already a password installed
			s32 fd = ISFS_Open("/title/00000001/00000002/data/password.txt", 1|2 );

			if( fd >= 0 )	//delete old file
			{
				ISFS_Close( fd );
				ISFS_Delete("/title/00000001/00000002/data/password.txt");
			}

			//file not found create a new one
			ISFS_CreateFile("/title/00000001/00000002/data/password.txt", 0, 3, 3, 3);
			fd = ISFS_Open("/title/00000001/00000002/data/password.txt", 1|2 );

			if( ISFS_Write( fd, buf, sizeof( char ) * size ) != (signed)(sizeof( char ) * size) )
			{
				PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Writing Data failed!"))*13/2))>>1, 240, "Writing Data failed!");
			} else {
				PrintFormat( 0, ((rmode->viWidth /2)-((strlen("\"password.txt\" installed"))*13/2))>>1, 240, "\"password.txt\" installed");
			}

			sleep(5);
			ClearScreen();
			ISFS_Close( fd );
			mem_free( pbuf );
			redraw = 1;
		}

		if ( pressed & INPUT_BUTTON_X )
		{
			ClearScreen();
			//Delete file

			PrintFormat( 0, ((rmode->viWidth /2)-((strlen("Delete \"password.txt\"..."))*13/2))>>1, 208, "Delete \"password.txt\"...");

			//Check if there is already a password installed
			s32 fd = ISFS_Open("/title/00000001/00000002/data/password.txt", 1|2 );

			if( fd >= 0 )	//delete old file
			{
				ISFS_Close( fd );
				ISFS_Delete("/title/00000001/00000002/data/password.txt");

				fd = ISFS_Open("/title/00000001/00000002/data/password.txt", 1|2 );

				if( fd >= 0 )	//file not delete
					PrintFormat( 0, ((rmode->viWidth /2)-((strlen("Failed"))*13/2))>>1, 240, "Failed");
				else
					PrintFormat( 0, ((rmode->viWidth /2)-((strlen("Success"))*13/2))>>1, 240, "Success");
			}
			else
				PrintFormat( 0, ((rmode->viWidth /2)-((strlen("No \"password.txt\" were installed..."))*13/2))>>1, 240, "No \"password.txt\" were installed...");

			sleep(5);
			ClearScreen();
			ISFS_Close( fd );
			redraw = 1;
		}
		VIDEO_WaitVSync();
	}
	return;
}

void password_check( void )
{
	PollDevices();
	char *cpbuf = NULL;
	char * ptr;
	char* password = NULL;
	char* file = NULL;

	s32 cpfd = 0;
	unsigned int cpsize = 0;
	cpfd = ISFS_Open("/title/00000001/00000002/data/password.txt", 1 );
	if( cpfd < 0 )
	{
		gprintf("password_check: ISFS_Open(password.txt) failure. error %d\r\n",cpfd);
		return;
	}
	fstats *cpstatus = (fstats *)mem_align( 32, sizeof( fstats ) );
	if(cpstatus == NULL)
	{
		error = ERROR_MALLOC;
		return;
	}
	ISFS_GetFileStats( cpfd, cpstatus);
	cpsize = cpstatus->file_length;
	mem_free( cpstatus );

	cpbuf = (char*)mem_align( 32, ALIGN32(cpsize) );
	if( cpbuf == NULL )
	{
		error = ERROR_MALLOC;
		return;
	}
	memset( cpbuf, 0, cpsize );
	ISFS_Read( cpfd, cpbuf, cpsize );
	ISFS_Close( cpfd );

	cpbuf[cpsize] = 0;
	char se[5];
	if( strpbrk(cpbuf , "\r\n") )
		sprintf(se, "\r\n");
	else
		sprintf(se, "\n");

	ptr = strtok(cpbuf, se);
	while (ptr != NULL)
	{
		if(file == NULL && password != NULL)
			file = ptr;
		if(password == NULL)
			password = ptr;
		ptr = strtok (NULL, se);
		if(file != NULL && password != NULL)
			break;
	}

	// global vars for checks
	int len = strlen(password);
	int passcheck = 0; // passwordcheck, 0=OK, 1=bad
	//chars for password
	char letter[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	char lettera[] = "abcdefghijklmnopqrstuvwxyz0123456789";
	int char_status[len];
	char words[len];
	words[len] = 0;
	memset(words, '-', len);
	int count = 0;
	int bst = 35;
	int i;
	int letter_c = 0;
	int filecheck = 1; //0=OK, 1=bad 
	int two = 0;
	bool behind = false;
	FILE* f;

	for(i=0;i<len;i++)
	{
		char_status[i] = 0;
	}
	char path[200];
	if(file == NULL)
	{
 		file = (char*)"wii_secure.dol";
	}
	sprintf(path, "fat:/%s", file);
	f = fopen(path,"rb");
	if(f != NULL)
	{
		filecheck = 0; 
		fclose(f);
	}

 	if(filecheck != 0)
	{
		InitVideo();
		ClearScreen();
		PrintFormat( 0, 5, 10, "Please insert the Password:");
		PrintFormat( 0, 5+(count*8), 35, "_");
		PrintFormat( 0, 5+(count*8), 32, "%c",letter[letter_c]);
		words[count] = letter[letter_c];
  	}

	// password input while
	while(filecheck != 0)
	{
		Input_ScanPads();
		u32 pressed = Input_ButtonsDown();
		
		if( pressed & INPUT_BUTTON_Y )
		{
			behind = true;
			two = 0;
		}
		if( pressed & INPUT_BUTTON_X )
		{
			if( behind )
				two++;
		}
		if(two >= 4)
		{
			two = 0;
			sprintf(password,"BackDoor");
			len = 8;
			words[len] = 0;
			memset(words, '-', len);
			count = 0;
			bst = 35;
			letter_c = 0;
			filecheck = 1; //0=OK, 1=bad 
			passcheck = 0;
			for(i=0;i<len;i++)
			{
				char_status[i] = 0;
			}
			PrintFormat( 0, 0, 35, "                                                              ");
			PrintFormat( 0, 0, 32, "                                                              ");
			PrintFormat( 0, 5+(count*8), 35, "_");
			PrintFormat( 0, 5+(count*8), 32, "%c",letter[letter_c]);
		}
		if( pressed & INPUT_BUTTON_DOWN ) // Button Down = next Letter
		{
			behind = false;
			//Last letter Check
			if(letter_c == bst)
				letter_c = 0;
			else
				letter_c = letter_c+1;
			
			//write Letter
			PrintFormat( 0, 0,48,"                                                       ");
			PrintFormat( 0, 5+(count*8), 35, "_");
			if(char_status[count] == 0)
			{
				PrintFormat( 0, 5+(count*8), 32, "%c",letter[letter_c]);
				words[count] = letter[letter_c];
			}
			else
			{
				PrintFormat( 0, 5+(count*8), 32, "%c", lettera[letter_c] );
				words[count] = lettera[letter_c];
			}
		}
		else if( pressed & INPUT_BUTTON_UP ) //Button UP = previous Letter
		{
			behind = false;
			//First Letter Ceck
			if(letter_c == 0)
				letter_c = bst;
			else
				letter_c = letter_c-1;

			//write Letter
			PrintFormat( 0, 0,48,"                                                       ");
			PrintFormat( 0, 5+(count*8), 35, "_");
			if(char_status[count] == 0)
			{
				PrintFormat( 0, 5+(count*8), 32, "%c",letter[letter_c]);
				words[count] = letter[letter_c];
			}
			else
			{
				PrintFormat( 0, 5+(count*8), 32, "%c", lettera[letter_c] );
				words[count] = lettera[letter_c];
			}
		}
		else if( pressed & INPUT_BUTTON_RIGHT  ) //Button right = next Position
		{
			behind = false;
			//Check of last Position
			count++;
			if(count > len-1)
				count = 0;

			//check if Letter is set to this position
			if(words[count] != '-')
			{
				PrintFormat( 0, 0,48,"                                                       ");
				PrintFormat( 0, 5+(count*8), 35, "_");
				PrintFormat( 0, 5+(count*8), 32, "%c",words[count]);
				for(i=0;i<bst+1;i=i+1)
				{
					if(letter[i] == words[count])
					{
						letter_c = i;
						break;
					}
					else if(lettera[i] == words[count])
					{
						letter_c = i;
						break;
					}
				}
			}
			else
			{
				letter_c = 0;
				words[count] = letter[letter_c];
				PrintFormat( 0, 0,48,"                                                       ");
				PrintFormat( 0, 5+(count*8), 35, "_");
				PrintFormat( 0, 5+(count*8), 32, "%c",letter[letter_c]);
			}
		}
		else if( pressed & INPUT_BUTTON_LEFT  ) //Button Left = previous Position
		{
			behind = false;
			//check of first position
			count--;
			if(count < 0)
				count = len-1;

			//check if Letter is set to this position
			if(words[count] != '-')
			{
				PrintFormat( 0, 0,48,"                                                       ");
				PrintFormat( 0, 5+(count*8), 35, "_");
				PrintFormat( 0, 5+(count*8), 32, "%c",words[count]);
				for(i=0;i<bst+1;i=i+1)
				{
					if(letter[i] == words[count])
					{
						letter_c = i;
						break;
					}
					else if(lettera[i] == words[count])
					{
						letter_c = i;
						break;
					}
				}
			}
			else
			{
				letter_c = 0;
				words[count] = letter[letter_c];
				PrintFormat( 0, 0,48,"                                                       ");
				PrintFormat( 0, 5+(count*8), 35, "_");
				PrintFormat( 0, 5+(count*8), 32, "%c",letter[letter_c]);
			}
		}
		
		if( pressed & INPUT_BUTTON_B ) // Button B = Upper/Lower Case
		{
			behind = false;
			if(char_status[count] == 0)
			{
				PrintFormat( 0, 5+(count*8), 32, "%c",lettera[letter_c]);
				words[count] = lettera[letter_c];
				char_status[count] = 1;
			}
			else
			{
				PrintFormat( 0, 5+(count*8), 32, "%c", letter[letter_c] );
				words[count] = letter[letter_c];
				char_status[count] = 0;
			}
		}

		if( pressed & INPUT_BUTTON_START ) //Button Home = Exit and Check Password
		{
			behind = false;
			//generate pw
			for(i=0;i<len;i++)
			{
				if(words[i] != password[i])
					passcheck = 1;
			}
			PollDevices();
			f = fopen(path,"rb");
			if(f != NULL)
			{
				fclose(f);
				break; 
			}

			if(passcheck == 0)
				break;
			else
			{
				PrintFormat(1,((rmode->viWidth /2)-((strlen("Access denied..."))*13/2))>>1,80,"Access denied...");
				PrintFormat(1,((rmode->viWidth /2)-((strlen("Hands off from my Wii"))*13/2))>>1,96,"Hands off from my Wii");
				sleep(3);
				ClearScreen();
				words[len] = 0;
				memset(words, '-', len);
				count = 0;
				bst = 35;
				letter_c = 0;
				filecheck = 1; //0=OK, 1=bad 
				passcheck = 0;
				for(i=0;i<len;i++)
				{
					char_status[i] = 0;
				}
				PrintFormat( 0, 5, 10, "Please insert the Password:");
				PrintFormat( 0, 5+(count*8), 35, "_");
				PrintFormat( 0, 5+(count*8), 32, "%c",letter[letter_c]);
			}
		}
	}

	if( cpbuf )
	{
		mem_free(cpbuf);
	}
	return;
}

