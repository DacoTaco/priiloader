#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <string.h>
#include <malloc.h>
#include <ogc/machine/processor.h>

#include "elf.h"
#include "gecko.h"

#define MAGIC_WORD_ADDRESS 0x817FEFF0 //0x8132FFFB

static void *xfb = NULL;
static GXRModeObj *vmode = NULL;
//to load the dol but its stupid cause of the autoboot...
//#define LOAD_DOL

s32 __IOS_LoadStartupIOS()
{
        return 0;
}
typedef struct {
	unsigned int offsetText[7];
	unsigned int offsetData[11];
	unsigned int addressText[7];
	unsigned int addressData[11];
	unsigned int sizeText[7];
	unsigned int sizeData[11];
	unsigned int addressBSS;
	unsigned int sizeBSS;
	unsigned int entrypoint;
} dolhdr;
void LoadThroughDol()
{
	void (*entrypoint)();
	if (ISFS_Initialize() < 0)
	{
		gprintf("Failed to get root\n");
		return;
	}
	static u64 TitleID ATTRIBUTE_ALIGN(32)=0x0000000100000002LL;
	static u32 tmd_size ATTRIBUTE_ALIGN(32);
	s32 r = 0;

	r=ES_GetStoredTMDSize(TitleID, &tmd_size);
	if(r<0)
	{
		gprintf("failed to get TMD size\n");
		return;
	}

	signed_blob *TMD = (signed_blob *)memalign( 32, (tmd_size+32)&(~31) );
	if( TMD == NULL )
	{
		gprintf("failed to allocate TMD\n");
		return;
	}
	memset(TMD, 0, tmd_size);

	r=ES_GetStoredTMD(TitleID, TMD, tmd_size);
	if(r<0)
	{
		free( TMD );
		gprintf("failed to get TMD\n");
		return;
	}
	
	tmd *rTMD = (tmd *)(TMD+(0x140/sizeof(tmd *)));
#ifdef DEBUG
	printf("num_contents:%08X\n", rTMD->num_contents );
#endif

	//get main.dol filename
	u32 fileID = 0;
	for(u32 z=0; z < rTMD->num_contents; ++z)
	{
		if( rTMD->contents[z].index == rTMD->boot_index )
		{
#ifdef DEBUG
			printf("%d:%d\n", rTMD->contents[z].index, rTMD->contents[z].cid);
#endif
			fileID = rTMD->contents[z].cid;
			break;
		}
	}

	if( fileID == 0 )
	{
		free( TMD );
		gprintf("failed to get main dol of system menu\n");
		return;
	}


	char * file = (char*)memalign( 32, 256 );
	if( file == NULL )
	{
		free( TMD );
		gprintf("failed to allocate file string\n");
		return;
	}

	memset(file, 0, 256 );

	sprintf( file, "/title/00000001/00000002/content/%08x.app", fileID );
	gprintf("file is %s\n",file);
	s32 fd = ISFS_Open(file, 1 );
	if( fd < 0 )
	{
		gprintf("failed to open app file\n");
		return;
	}

	Elf32_Ehdr *ElfHdr = (Elf32_Ehdr *)memalign( 32, (sizeof( Elf32_Ehdr )+32)&(~31) );
	if( ElfHdr == NULL )
	{
		gprintf("failed to allign elf header\n");
		return;
	}

	r = ISFS_Read( fd, ElfHdr, sizeof( Elf32_Ehdr ) );
	if( r < 0 || r != sizeof( Elf32_Ehdr ) )
	{
#ifdef DEBUG
		sleep(10);
#endif
		gprintf("read error\n");
		return;
	}
	dolhdr *hdr = (dolhdr *)memalign(32, (sizeof( dolhdr )+32)&(~31) );
	if( hdr == NULL )
	{
		gprintf("dol header allign error\n");
		return;
	}

	r = ISFS_Seek( fd, 0, 0);
	if( r < 0 )
	{
		gprintf("ISFS_Read failed:%d\n", r);
		//sleep(5);
		//exit(0);
		return;
	}

	r = ISFS_Read( fd, hdr, sizeof(dolhdr) );

	if( r < 0 || r != sizeof(dolhdr) )
	{
		gprintf("ISFS_Read failed:%d\n", r);
		//sleep(5);
		//exit(0);
		return;
	}

	//printf("read:%d\n", r );

	gprintf("\nText Sections:\n");

	int i=0;
	for (i = 0; i < 6; i++)
	{
		if( hdr->sizeText[i] && hdr->addressText[i] && hdr->offsetText[i] )
		{
			if(ISFS_Seek( fd, hdr->offsetText[i], SEEK_SET )<0)
			{
				gprintf("seek error");
				return;
			}
			
			//if( hdr->addressText[i] & (~31) )
			//{
			//	u8 *tbuf = (u8*)memalign(32, (hdr->sizeText[i]+32)&(~31) );

			//	ISFS_Read( fd, tbuf, hdr->sizeText[i]);

			//	memcpy( (void*)(hdr->addressText[i]), tbuf, hdr->sizeText[i] );

			//	free( tbuf);

			//} else {
				if(ISFS_Read( fd, (void*)(hdr->addressText[i]), hdr->sizeText[i] )<0)
				{
					gprintf("read after seek error\n");
					return;
				}
			//}
			DCInvalidateRange( (void*)(hdr->addressText[i]), hdr->sizeText[i] );

			gprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr->offsetText[i]), hdr->addressText[i], hdr->sizeText[i]);
		}
	}

	gprintf("\nData Sections:\n");

	// data sections
	for (i = 0; i <= 10; i++)
	{
		if( hdr->sizeData[i] && hdr->addressData[i] && hdr->offsetData[i] )
		{
			if(ISFS_Seek( fd, hdr->offsetData[i], SEEK_SET )<0)
			{
				gprintf("seek offsetdata error\n");
				return;
			}
			
			//if( hdr->addressData[i] & (~31) )
			//{
			//	u8 *tbuf = (u8*)memalign(32, (hdr->sizeData[i]+32)&(~31) );

			//	ISFS_Read( fd, tbuf, hdr->sizeData[i]);

			//	memcpy( (void*)(hdr->addressData[i]), tbuf, hdr->sizeData[i] );

			//	free( tbuf);

			//} else {
				if( ISFS_Read( fd, (void*)(hdr->addressData[i]), hdr->sizeData[i] )<0)
				{
					gprintf("read offsetdata error\n");
					return;
				}
			//}

			DCInvalidateRange( (void*)(hdr->addressData[i]), hdr->sizeData[i] );

			gprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (hdr->offsetData[i]), hdr->addressData[i], hdr->sizeData[i]);
		}
	}

	entrypoint = (void (*)())(hdr->entrypoint);

	if( entrypoint == 0x00000000 )
	{
		return;
	}
	free(TMD);
	for(i=0;i<WPAD_MAX_WIIMOTES;i++) {
		WPAD_Flush(i);
		WPAD_Disconnect(i);
	}
	WPAD_Shutdown();
	ISFS_Deinitialize();
	gprintf("Entrypoint: %08X\n", (u32)(entrypoint) );
	__IOS_ShutdownSubsystems();
	u32 level;
	SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	_CPU_ISR_Disable (level);
	mtmsr(mfmsr() & ~0x8000);
	mtmsr(mfmsr() | 0x2002);
	entrypoint();
	_CPU_ISR_Restore (level);
	//never gonna happen; but failsafe
	ISFS_Initialize();
	return;
}

void LoadThroughMagicWord()
{
	//retarded that this is the only way without touching the settings of priiloader or load the dol...
	printf("magic word is %x\n",*(vu32*)MAGIC_WORD_ADDRESS);
	*(vu32*)MAGIC_WORD_ADDRESS = 0x4461636f; // "Daco" , causes priiloader to skip autoboot and load the priiloader menu
	//*(vu32*)MAGIC_WORD_ADDRESS = 0x50756e65; // "Pune" , causes priiloader to skip autoboot and load Sys Menu
	DCFlushRange((void*)MAGIC_WORD_ADDRESS, 4);
	printf("magic word changed to %x\n",*(vu32*)MAGIC_WORD_ADDRESS);

	printf("resetting...\n");
	sleep(2);
	SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
}
//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	CheckForGecko();
	// Initialise the video system
	VIDEO_Init();

	vmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(vmode));

	VIDEO_Configure(vmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(false);
	VIDEO_Flush();

	VIDEO_WaitVSync();
	if (vmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

	int x = 20, y = 20, w, h;
	w = vmode->fbWidth - (x * 2);
	h = vmode->xfbHeight - (y + 20);

	// Initialize the console
	CON_InitEx(vmode, x, y, w, h);

	VIDEO_ClearFrameBuffer(vmode, xfb, COLOR_BLACK);
    
	// This function initialises the attached controllers
	WPAD_Init();
	
	// Call WPAD_ScanPads each loop, this reads the latest controller states
	WPAD_ScanPads();

	// WPAD_ButtonsDown tells us which buttons were pressed in this loop
	// this is a "one shot" state which will not fire again until the button has been released
	u32 pressed = WPAD_ButtonsDown(0);

	// We return to the launcher application via exit
	if ( pressed & WPAD_BUTTON_HOME ) exit(0);
	sleep(1);

#ifndef LOAD_DOL
	LoadThroughMagicWord();
#else
	LoadThroughDol();
#endif

	return 0;
}
