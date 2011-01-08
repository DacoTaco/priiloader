#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <gccore.h>
#include <malloc.h>
#include <wiiuse/wpad.h>
#include <ogc/machine/processor.h>
#include <ogc/machine/asm.h>

#include "elf.h"

#include "boot_dol.h"

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

#define gdprintf gprintf

s8 GeckoFound = 0;

void CheckForGecko( void )
{
	GeckoFound = usb_isgeckoalive( EXI_CHANNEL_1 );
	if(GeckoFound)
		usb_flush(EXI_CHANNEL_1);
	return;
}
void gprintf( const char *str, ... )
{
	if(!GeckoFound)
		return;

	char astr[4096];

	va_list ap;
	va_start(ap,str);
	vsprintf( astr, str, ap );
	va_end(ap);

	if(GeckoFound)
	{
		usb_sendbuffer( 1, astr, strlen(astr) );
		usb_flush(EXI_CHANNEL_1);
	}
	return;
}
s8 BootDolFromMem( u8 *dolstart , u8 HW_AHBPROT_ENABLED ) 
{
	if(dolstart == NULL)
		return -1;

    u32 i;
	void	(*entrypoint)();
	
	Elf32_Ehdr ElfHdr;

	memcpy(&ElfHdr,dolstart,sizeof(Elf32_Ehdr));

	if( ElfHdr.e_ident[EI_MAG0] == 0x7F ||
		ElfHdr.e_ident[EI_MAG1] == 'E' ||
		ElfHdr.e_ident[EI_MAG2] == 'L' ||
		ElfHdr.e_ident[EI_MAG3] == 'F' )
	{
		gdprintf("BootDolFromMem : ELF Found\n");
		gdprintf("Type:      \t%04X\n", ElfHdr.e_type );
		gdprintf("Machine:   \t%04X\n", ElfHdr.e_machine );
		gdprintf("Version:  %08X\n", ElfHdr.e_version );
		gdprintf("Entry:    %08X\n", ElfHdr.e_entry );
		gdprintf("Flags:    %08X\n", ElfHdr.e_flags );
		gdprintf("EHsize:    \t%04X\n\n", ElfHdr.e_ehsize );

		gdprintf("PHoff:    %08X\n",	ElfHdr.e_phoff );
		gdprintf("PHentsize: \t%04X\n",	ElfHdr.e_phentsize );
		gdprintf("PHnum:     \t%04X\n\n",ElfHdr.e_phnum );

		gdprintf("SHoff:    %08X\n",	ElfHdr.e_shoff );
		gdprintf("SHentsize: \t%04X\n",	ElfHdr.e_shentsize );
		gdprintf("SHnum:     \t%04X\n",	ElfHdr.e_shnum );
		gdprintf("SHstrndx:  \t%04X\n\n",ElfHdr.e_shstrndx );
		if( ElfHdr.e_phnum == 0 )
		{
			gdprintf("BootDolFromMem : Warning program header entries are zero!\n");
		} 
		else 
		{
			for( s32 i=0; i < ElfHdr.e_phnum; ++i )
			{
				Elf32_Phdr phdr;
				ICInvalidateRange (&phdr ,sizeof( phdr ) );
				memmove(&phdr,dolstart + (ElfHdr.e_phoff + sizeof( Elf32_Phdr ) * i) ,sizeof( phdr ) );
				gdprintf("Type:%08X Offset:%08X VAdr:%08X PAdr:%08X FileSz:%08X\n", phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_paddr, phdr.p_filesz );
				ICInvalidateRange ((void*)(phdr.p_vaddr | 0x80000000),phdr.p_filesz);
				if(phdr.p_type == PT_LOAD )
					memmove((void*)(phdr.p_vaddr | 0x80000000), dolstart + phdr.p_offset , phdr.p_filesz);
			}
		}

		//according to dhewg the section headers are totally un-needed (infact, they break a few elf loading)
		//however, checking for the type does the trick to make them work :)
		if( ElfHdr.e_shnum == 0 )
		{
			gdprintf("BootDolFromMem : Warning section header entries are zero!\n");
		} 
		else 
		{

			for( s32 i=0; i < ElfHdr.e_shnum; ++i )
			{

				Elf32_Shdr shdr;
				memmove(&shdr, dolstart + (ElfHdr.e_shoff + sizeof( Elf32_Shdr ) * i) ,sizeof( shdr ) );
				DCFlushRangeNoSync(&shdr ,sizeof( shdr ) );

				if( shdr.sh_type == SHT_NULL )
					continue;

				if( shdr.sh_type > SHT_GROUP )
					gdprintf("Warning the type: %08X could be invalid!\n", shdr.sh_type );

				if( shdr.sh_flags & ~0xF0000007 )
					gdprintf("Warning the flag: %08X is invalid!\n", shdr.sh_flags );

				gdprintf("Type:%08X Offset:%08X Name:%08X Off:%08X Size:%08X\n", shdr.sh_type, shdr.sh_offset, shdr.sh_name, shdr.sh_addr, shdr.sh_size );
				if (shdr.sh_type == SHT_NOBITS)
				{
					memmove((void*)(shdr.sh_addr | 0x80000000), dolstart + shdr.sh_offset,shdr.sh_size);
					DCFlushRangeNoSync((void*)(shdr.sh_addr | 0x80000000),shdr.sh_size);
				}
			}
		}
		entrypoint = (void (*)())(ElfHdr.e_entry | 0x80000000);
	}
	else
	{
		gdprintf("BootDolFromMem : DOL detected\n");
		dolhdr *dolfile;
		dolfile = (dolhdr *) dolstart;
		for (i = 0; i < 7; i++) {
			if ((!dolfile->sizeText[i]) || (dolfile->addressText[i] < 0x100)) continue;
			ICInvalidateRange ((void *) dolfile->addressText[i],dolfile->sizeText[i]);
			memmove ((void *) dolfile->addressText[i],dolstart+dolfile->offsetText[i],dolfile->sizeText[i]);
		}
		gdprintf("Data Sections :\n");
		for (i = 0; i < 11; i++) {
			if ((!dolfile->sizeData[i]) || (dolfile->offsetData[i] < 0x100)) continue;
			memmove ((void *) dolfile->addressData[i],dolstart+dolfile->offsetData[i],dolfile->sizeData[i]);
			DCFlushRangeNoSync ((void *) dolfile->offsetData[i],dolfile->sizeData[i]);
			gdprintf("\t%08x\t\t%08x\t\t%08x\t\t\n", (dolfile->offsetData[i]), dolfile->addressData[i], dolfile->sizeData[i]);
		}

		memset ((void *) dolfile->addressBSS, 0, dolfile->sizeBSS);
		DCFlushRange((void *) dolfile->addressBSS, dolfile->sizeBSS);
		entrypoint = (void (*)())(dolfile->entrypoint);
	}
	if(entrypoint == 0x00000000 )
	{
		gprintf("BootDolFromMem : bogus entrypoint of %08X detected\n",(u32)(entrypoint));
		return -2;
	}
	gprintf("BootDolFromMem : starting binary...\n");

	if(!HW_AHBPROT_ENABLED || read32(0x0d800064) != 0xFFFFFFFF )
	{
		IOS_ReloadIOS(36);
	}
	
	gprintf("BootDolFromMem : Entrypoint: %08X\n", (u32)(entrypoint) );

	__STM_Close();
	__IOS_ShutdownSubsystems();
	mtmsr(mfmsr() & ~0x8000);
	mtmsr(mfmsr() | 0x2002);
	ICSync();
	entrypoint();

	//it failed. FAIL!
	__IOS_InitializeSubsystems();
	gprintf("BootDolFromMem : booting failure\n");
	return -1;
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// Initialise the video system
	/*VIDEO_Init();

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
	WPAD_Init();*/
	
	gprintf("booting dol...\n");
	sleep(2);
	BootDolFromMem((u8*)boot_dol,0);
	gprintf("shit failed\n");

	return 0;
}
