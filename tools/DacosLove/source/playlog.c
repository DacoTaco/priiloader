/*
	PLAYLOG.C
	This code allows to modify play_rec.dat in order to store the
	game time in Wii's log correctly.

	by Marc
	Thanks to tueidj for giving me some hints on how to do it :)
	Most of the code was taken from here:
	http://forum.wiibrew.org/read.php?27,22130
*/

#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include "playlog.h"

#define SECONDS_TO_2000 946684800LL
#define TICKS_PER_SECOND 60750000LL
#ifndef PLAYRECPATH
	#define PLAYRECPATH "/title/00000001/00000002/data/play_rec.dat"
#endif
typedef struct
{
	u32 checksum;
	union
	{
		u32 data[31];
		struct
		{
			u8 name[84];
			u64 ticks_boot;
			u64 ticks_last;
			char title_id[6];
			char unknown[18];
		} ATTRIBUTE_PACKED;
	};
} playrec_struct;

playrec_struct playrec_buf;

// Thanks to Dr. Clipper
u64 getWiiTime(void)
{
	time_t uTime;
	uTime = time(NULL);
	return TICKS_PER_SECOND * (uTime - SECONDS_TO_2000);
}

int Playlog_Update(const char ID[6], const u8 title[84])
{
	s32 ret,playrec_fd;
	u32 sum = 0;
	u8 i;
	u64 stime;
	playrec_struct playrec_buf;
	//Open play_rec.dat
	playrec_fd = IOS_Open(PLAYRECPATH, IPC_OPEN_RW);
	if(playrec_fd == -106)
		{
			IOS_Close(playrec_fd);
			
			//In case the play_rec.dat wasn´t found create one and try again
			int ret = ISFS_CreateFile(PLAYRECPATH,0,3,3,3);
			if( ret < 0 )
				goto error_1;
				
			playrec_fd = IOS_Open(PLAYRECPATH, IPC_OPEN_RW);
			if(playrec_fd < 0)
				goto error_1;
		}
	else if(playrec_fd < 0)
		goto error_1;

    stime = getWiiTime();
	playrec_buf.ticks_boot = stime;
	playrec_buf.ticks_last = stime;

	//Update channel name and ID
	memcpy(playrec_buf.name, title, 84);
	memcpy(playrec_buf.title_id, ID, 6);
	
	memset(playrec_buf.unknown, 0, 18);

	//Calculate and update checksum
	for(i=0; i<31; i++)
		sum += playrec_buf.data[i];
	playrec_buf.checksum=sum;

	//Write play_rec.dat
	ret = IOS_Write(playrec_fd, &playrec_buf, sizeof(playrec_buf));
	if(ret!=sizeof(playrec_buf))
		goto error_2;

	IOS_Close(playrec_fd);
	return 0;

error_1:
	IOS_Close(playrec_fd);
	return -1;

error_2:
	IOS_Close(playrec_fd);
	return -2;
}

int Playlog_Delete(void)
{
	s32 ret,playrec_fd;
	
	//Open play_rec.dat
	playrec_fd = IOS_Open(PLAYRECPATH, IPC_OPEN_RW);
	if(playrec_fd < 0)
		goto error_1;

	//Read play_rec.dat
	ret = IOS_Read(playrec_fd, &playrec_buf, sizeof(playrec_buf));
	if(ret != sizeof(playrec_buf))
		goto error_2;

	if(IOS_Seek(playrec_fd, 0, 0)<0)
		goto error_2;
    
	// invalidate checksum
	playrec_buf.checksum=0;

	ret = IOS_Write(playrec_fd, &playrec_buf, sizeof(playrec_buf));
	if(ret!=sizeof(playrec_buf))
		goto error_2;

	IOS_Close(playrec_fd);
	return 0;

error_1:
	IOS_Close(playrec_fd);

error_2:
	return -1;
}
