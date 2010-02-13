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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <malloc.h>

#include "font.h"
#include "gecko.h"
#include "error.h"


extern u8 error;
GXRModeObj *rmode = VIDEO_GetPreferredMode(NULL);
extern void ClearScreen();
extern bool RemountDevices();

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
	if (!RemountDevices() )
	{
		PrintFormat( 1, ((rmode->viWidth /2)-((strlen("Failed to mount fat device!"))*13/2))>>1, 208, "Failed to mount fat device!");
		sleep(5);
		return;
	}

	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();


		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);

		if( redraw )
		{
			password = NULL;
			file = NULL;
			if( ISFS_Initialize() == 0 )
			{
				s32 pfd=0;
				pfd = ISFS_Open("/title/00000001/00000002/data/password.txt", 1 );
				if( pfd < 0 )
				{
					gprintf("password.txt not found on NAND. ISFS_Open returns %d\n",pfd);
				}
				else
				{
					fstats *pstatus = (fstats *)memalign( 32, sizeof( fstats ) );
					ISFS_GetFileStats( pfd, pstatus);
					psize = pstatus->file_length;
					free( pstatus );
					free( pbuf );
					pbuf = (char*)memalign( 32, (psize+32+1)&(~31) );
					if( pbuf == NULL )
					{
						error = ERROR_MALLOC;
						return;
					}
					memset( pbuf, 0, (psize+32+1)&(~31) );
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
			if( in ) // if( RemountDevices() && in )
				PrintFormat( 1, (rmode->viWidth-48)>>1, 48, "OK");
			else
				PrintFormat( 0, (rmode->viWidth-48)>>1, 48, "--");
			fclose(in);

			PrintFormat( 0, ((rmode->viWidth /2)-((strlen("A(A) Install file"))*13/2))>>1, rmode->viHeight-64, "A(A) Install File");
			PrintFormat( 0, ((rmode->viWidth /2)-((strlen("2(X) Delete File "))*13/2))>>1, rmode->viHeight-48, "2(X) Delete File");
			redraw = 0;
		}

#ifdef DEBUG
		if ( (WPAD_Pressed & WPAD_BUTTON_HOME) || (PAD_Pressed & PAD_BUTTON_START) )
			exit(0);
#endif
		if ( (WPAD_Pressed & WPAD_BUTTON_B) || (PAD_Pressed & PAD_BUTTON_B) )
		{
			free(pbuf);
			break;
		}

		if ( (WPAD_Pressed & WPAD_BUTTON_A) || (PAD_Pressed & PAD_BUTTON_A) )
		{
			ClearScreen();
	//Install file
#ifdef libELM
			sprintf(filepath, "elm:/sd/password.txt");
			FILE *passtxt = fopen(filepath, "rb" );
#else
			sprintf(filepath, "fat:/password.txt");
			FILE *passtxt = fopen(filepath, "rb" );
#endif
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

			char *buf = (char*)memalign( 32, sizeof( char ) * size );
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
			free( pbuf );
			redraw = 1;
		}

		if ( (WPAD_Pressed & WPAD_BUTTON_2) || (PAD_Pressed & PAD_BUTTON_X) )
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
	}
	return;
}

void Pad_unpressed( void )
{
	while(1)
	{
		PAD_ScanPads();
		u32 PAD_Unpressed  = PAD_ButtonsUp(0) | PAD_ButtonsUp(1) | PAD_ButtonsUp(2) | PAD_ButtonsUp(3);
		if ( PAD_Unpressed )
			break;
	}
	return;
}

void password_check( void )
{
	RemountDevices();
	char *cpbuf = NULL;
	char * ptr;
	char* password = NULL;
	char* file = NULL;

	s32 cpfd = 0;
	unsigned int cpsize = 0;
	cpfd = ISFS_Open("/title/00000001/00000002/data/password.txt", 1 );
	if( cpfd < 0 )
	{
		gprintf("password.txt not found on NAND. ISFS_Open returns %d\n",cpfd);
		return;
	}
	fstats *cpstatus = (fstats *)memalign( 32, sizeof( fstats ) );
	ISFS_GetFileStats( cpfd, cpstatus);
	cpsize = cpstatus->file_length;
	if( cpstatus )
		free( cpstatus );
	if( cpbuf )	
		free( cpbuf );
	cpbuf = (char*)memalign( 32, (cpsize+32+1)&(~31) );
	if( cpbuf == NULL )
	{
		error = ERROR_MALLOC;
		return;
	}
	memset( cpbuf, 0, (cpsize+32+1)&(~31) );
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
	FILE* f;

	for(i=0;i<len;i++)
	{
		char_status[i] = 0;
	}
	char path[200];
	if(file == NULL)
	{
 		file = "wii_secure.dol";
	}
	sprintf(path, "fat:/%s", file);
	f = fopen(path,"rb");
	if(f != NULL)
		filecheck = 0; 
	fclose(f);

 	if(filecheck != 0)
	{
		ClearScreen();
		PrintFormat( 0, 5, 10, "Please insert the Password:");
		PrintFormat( 0, 5+(count*8), 35, "_");
		PrintFormat( 0, 5+(count*8), 32, "%c",letter[letter_c]);
		words[count] = letter[letter_c];
		PAD_Init();
		WPAD_Init();
  	}

	// password input while
	while(filecheck != 0)
	{
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);

		if( (WPAD_Pressed & WPAD_BUTTON_DOWN) || (PAD_Pressed & PAD_BUTTON_DOWN) ) // Button Down = next Letter
		{
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
			if( PAD_Pressed )
				Pad_unpressed();
		}
		else if( (WPAD_Pressed & WPAD_BUTTON_UP) || (PAD_Pressed & PAD_BUTTON_UP) ) //Button UP = previus Letter
		{
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
			if( PAD_Pressed )
				Pad_unpressed();
		}
		else if( (WPAD_Pressed & WPAD_BUTTON_RIGHT) || (PAD_Pressed & PAD_BUTTON_RIGHT)  ) //Button right = next Position
		{
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
			if( PAD_Pressed )
				Pad_unpressed();
		}
		else if( (WPAD_Pressed & WPAD_BUTTON_LEFT) || (PAD_Pressed & PAD_BUTTON_LEFT)  ) //Button Left = previus Position
		{
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
			if( PAD_Pressed )
				Pad_unpressed();
		}
		
		if( (WPAD_Pressed & WPAD_BUTTON_B) || (PAD_Pressed & PAD_BUTTON_B) ) // Button B = Upper/Lower Case
		{
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
			if( PAD_Pressed )
				Pad_unpressed();
		}

		if( (WPAD_Pressed & WPAD_BUTTON_HOME) || (PAD_Pressed & PAD_BUTTON_START) ) //Button Home = Exit and Check Password
		{
			//generate pw
			for(i=0;i<len;i++)
			{
				if(words[i] != password[i])
					passcheck = 1;
			}
			RemountDevices();
			f = fopen(path,"rb");
			if(f != NULL)
			{
				fclose(f);
				break; 
			}
			fclose(f);
			if(passcheck == 0)
				break;
			else
			{
				PrintFormat(1,((rmode->viWidth /2)-((strlen("Access denied..."))*13/2))>>1,80,"Access denied...");
				PrintFormat(1,((rmode->viWidth /2)-((strlen("Hands off from my Wi"))*13/2))>>1,96,"Hands off from my Wi");
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

	WPAD_Shutdown();
	if( cpbuf )
		free(cpbuf);
	return;
}

