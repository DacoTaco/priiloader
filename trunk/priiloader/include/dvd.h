/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2009 DacoTaco

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
#ifndef _DVD_H_
#define _DVD_H_

//headers

//typedefs
typedef struct DVD_status {
	s8 DriveClosed:4;
	s8 DriveError:4;
} DVD_status;

//variables

//functions
#ifdef __cplusplus
   extern "C" {
#endif

void DVDStopDisc( bool do_async );
void DVDCleanUp( void );
s8 DvdKilled( void );
//s32 GetDvdFD( void );

#ifdef __cplusplus
   }
#endif
#endif
