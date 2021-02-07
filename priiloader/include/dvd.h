/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019 DacoTaco

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
typedef struct _DVDTableOfContent
{
	u32 bootInfoCount;
	u32 partitionInfoOffset;
} DVDTableOfContent;

typedef struct _DVDPartitionInfo
{
	u32 offset;
	u32 len;
} DVDPartitionInfo;

typedef struct
{
	char revision[16];
	void* entry;
	s32 size;
	s32 trailersize;
	s32 padding;
} apploader_hdr [[gnu::aligned(32)]];

//apploader functions
typedef void (*dvd_main)(void);
typedef int (*apploader_main)(void** dst, int* size, int* offset);
typedef void* (*apploader_final)(void);
typedef void (*apploader_init_cb)(const char* fmt, ...);
typedef void (*apploader_init)(apploader_init_cb);
typedef void (*apploader_entry)(apploader_init* init, apploader_main* main, apploader_final* final);

#define DVD_RESETHARD				0			//Performs a hard reset. Complete new boot of FW.
#define DVD_RESETSOFT				1			//Performs a soft reset. FW restart and drive spinup
#define DVD_RESETNONE				2			//Only initiate DI registers

#define DVD_CMD_IDENTIFY			0x12
#define DVD_CMD_READ_ID				0x70
#define DVD_CMD_READ				0x71
#define DVD_CMD_RESET_DRIVE			0x8A
#define DVD_CMD_OPEN_PARTITION		0x8B
#define DVD_CMD_CLOSE_PARTITION		0x8C
#define DVD_CMD_UNENCRYPTED_READ	0x8D
#define DVD_CMD_STOP_DRIVE			0xE3
#define DVD_CMD_CONFIG_AUDIO_BUFFER	0xE4

#define GCDVD_MAGIC_VALUE			0xC2339F3D
#define WIIDVD_MAGIC_VALUE			0x5D1C9EA3
#define BC_Title_Id					0x0000000100000100ULL
#define WIIDVD_TOC_OFFSET			0x40000
#define APPLOADER_HDR_OFFSET		0x2440

//variables

//functions
#ifdef __cplusplus
   extern "C" {
#endif

s32 DVDInit(void);
s32 DVDAsyncBusy( void );
void DVDCloseHandle(void);
s32 DVDDiscAvailable( void );
s32 DVDStopDrive( void );
s32 DVDStopDriveAsync(void);
s32 DVDResetDrive( void );
s32 DVDResetDriveWithMode(u32 mode);
s32 DVDReadGameID(void* dst, u32 len);
s32 DVDUnencryptedRead(u32 offset, void* buf, u32 len);
s32 DVDOpenPartition(u32 offset, void* eticket, void* shared_cert_in, u32 shared_cert_in_len, void* tmd_out );
s32 DVDClosePartition();
s32 DVDRead(off_t offset, void* output, u32 len);
s32 DVDInquiry();

//Configures the drive's audio streaming buffer. also clears the DiscID from memory, but doesn't clear the cache.
s32 DVDAudioBufferConfig(u8 enable, s8 buffer_size);

//Command execution
s32 DVDExecuteCommand(u32 command, u8 do_async, void* input, s32 input_size, void* output, s32 output_size, ipccallback callback);
s32 DVDExecuteVCommand(s32 command, bool do_async, s32 cnt_in, s32 cnt_io, void* cmd_input, u32 cmd_input_size, void* input, u32 input_size, ipccallback callback, void* userdata);

#ifdef __cplusplus
   }
#endif
#endif
