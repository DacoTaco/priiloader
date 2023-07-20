/*
priiloader - A tool which allows to change the default boot up sequence on the Wii console
Executable Loader - Loads any executable who has been loaded into memory

Copyright (C) 2008-2019  DacoTaco

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

#include <ogc/machine/processor.h>
#include <gctypes.h>
#include "executables.h"
#include "patches.h"
#include "loader.h"

#ifndef MAX_ADDRESS
#define MAX_ADDRESS 0x817FFEFF
#endif

void DCFlushRangeNoGlobalSync(void *startaddress,u32 len);
void ICInvalidateRange(void* addr, u32 size);
void _memcpy(void* dst, void* src, u32 len);
void _memset(void* src, u32 data, u32 len);
u32 _loadApplication(u8* binary, void* parameter);

//NOTE : see loader.h about this function's definition & signature
void _boot (_LDR_PARAMETERS)
{
	if(binary == NULL || (parameter == NULL && parameterCount > 0))
		return;

	u32 ep = _loadApplication(binary,(binaryType == BINARY_TYPE_DEFAULT)?parameter:NULL);
	if( !ep || (binaryType == BINARY_TYPE_SYSTEM_MENU && ep != 0x80003400))
		return;

	//nintendo related pokes. TT/f0f claims these make official dols work
	*(vu32*)0x800000F8 = 0x0E7BE2C0;				// Bus Clock Speed
	*(vu32*)0x800000FC = 0x2B73A840;				// CPU Clock Speed
	*(vu32*)0x8000315C = 0x80800113;				// DI Legacy mode ?

	if (binaryType == BINARY_TYPE_SYSTEM_MENU || binaryType == BINARY_TYPE_SYSTEM_MENU_VWII)
	{
		/* apply offset patches*/
		if(parameter != NULL && parameterCount > 0)
		{
			offset_patch *patch = parameter;
			for(u32 i = 0;i < parameterCount;i++)
			{	
				_memcpy((void*)patch->offset,patch->patch,patch->patch_size);
				DCFlushRangeNoGlobalSync((void*)patch->offset, patch->patch_size);
				ICInvalidateRange((void*)patch->offset, patch->patch_size);
				patch = (offset_patch *)((8 + patch->patch_size) + (u32)patch);
			}
		}

		if(binaryType == BINARY_TYPE_SYSTEM_MENU)
		{
			mtmsr(mfmsr() & ~0x8000);
			mtmsr(mfmsr() | 0x2002);
			
			//unstub asm code by crediar. basically sets the SRR0 & SRR1
			//the rfi sets the machine state and starts executing @ SRR0
			asm(R"(isync
				#set MSR[DR:IR] = 00, jump to STUB
				lis 3,0x3400@h
				ori 3,3,0x3400@l
				mtsrr0 3

				mfmsr 3
				li 4,0x30
				andc 3,3,4
				mtsrr1 3
				rfi)");
		}
		else if (binaryType == BINARY_TYPE_SYSTEM_MENU_VWII)
		{
			// Running the entire stub of the vWii SM again crashes
			// This is a minimal version of this stub which directly jumps to the vWii SM entry
			asm("isync\n"

				// Write 0 to 0x800000f4 (usually 0x000000f4 without address translation)
				"lis 3, 0x800000f4@h\n"
				"lis 4, 0x0\n"
				"stw 4, 0x800000f4@l(3)\n"

				// Load the entry address into SSR0
				"lis 3, 0x81330400@h\n"
				"ori 3, 3, 0x81330400@l\n"
				"mtsrr0 3\n"

				// Prepare machine state
				"lis 4, 0x0\n"
				// Enable FP
				"addi 4, 4, 0x2000\n"
				// Enable IR and DR
				"ori 4, 4, 0x30\n"
				// Write machine state into SRR1
				"mtsrr1 4\n"

				// load SRR1 into MSR and jump to SRR0
				"rfi"
			);
		}

		__builtin_unreachable();
	}
	else // BINARY_TYPE_DEFAULT
	{
		asm("isync");
		//jump to entrypoint
		((void (*)())(ep))();
		__builtin_unreachable();
	}
}

u32 _loadApplication(u8* binary, void* parameter)
{
	Elf32_Ehdr *ElfHdr = (Elf32_Ehdr *)binary;
	dolhdr *dolfile = (dolhdr *)binary;
	EspressoAncastHeader *AncastHdr = (EspressoAncastHeader *)((u32)binary + dolfile->offsetData[0]);
	struct __argv *args = (struct __argv *)parameter;

	if( ElfHdr->e_ident[EI_MAG0] == 0x7F &&
		ElfHdr->e_ident[EI_MAG1] == 'E' &&
		ElfHdr->e_ident[EI_MAG2] == 'L' &&
		ElfHdr->e_ident[EI_MAG3] == 'F' )
	{
		if( (ElfHdr->e_entry | 0x80000000) < 0x80003400 && (ElfHdr->e_entry | 0x80000000) >= MAX_ADDRESS )
		{
			return 0;
		}

		for( s32 i=0; i < ElfHdr->e_phnum; ++i )
		{
			Elf32_Phdr* phdr = (Elf32_Phdr*)(binary + (ElfHdr->e_phoff + sizeof( Elf32_Phdr ) * i));
			ICInvalidateRange ((void*)(phdr->p_vaddr | 0x80000000),phdr->p_filesz);
			if(phdr->p_type != PT_LOAD )
				continue;

			//PT_LOAD Segment, aka static program data
			_memcpy((void*)(phdr->p_vaddr | 0x80000000), binary + phdr->p_offset , phdr->p_filesz);
		}

		//according to dhewg the section headers are totally un-needed (infact, they break a few elf loading)
		//this has to do with not clearing the SHT_NOBITS area's (BSS or memory used for uninitialised data)
		for( s32 i=0; i < ElfHdr->e_shnum; ++i )
		{
			Elf32_Shdr *shdr = (Elf32_Shdr*)(binary + (ElfHdr->e_shoff + sizeof( Elf32_Shdr ) * i));

			//useless check - null section = no purpose
			//if( shdr->sh_type == SHT_NULL )
			//	continue;

			if (shdr->sh_type != SHT_NOBITS)
				continue;
				
			_memset((void*)(shdr->sh_addr | 0x80000000), 0, shdr->sh_size);
			DCFlushRangeNoGlobalSync((void*)(shdr->sh_addr | 0x80000000),shdr->sh_size);
		}
		return (ElfHdr->e_entry | 0x80000000);	
	}
	// Note that ancast images passed to the loader need to be already decrypted
	else if (AncastHdr->header_block.magic == ANCAST_MAGIC)
	{
		void *dest = (void *)ESPRESSO_ANCAST_LOCATION_VIRT;
		u32 totalImageSize = AncastHdr->info_block.body_size + sizeof(EspressoAncastHeader);

		// Copy the ancast image to the location
		_memcpy(dest, AncastHdr, totalImageSize);
		DCFlushRangeNoGlobalSync(dest, totalImageSize);

		// Set the entrypoint to the body
		return ESPRESSO_ANCAST_LOCATION_VIRT + sizeof(EspressoAncastHeader);
	}
	else
	{
		u8 set_bss = (dolfile->addressBSS > 0x80003400 && dolfile->addressBSS + dolfile->sizeBSS < MAX_ADDRESS);

		//entrypoint & BSS checking
		if( (dolfile->entrypoint | 0x80000000) < 0x80003400 || (dolfile->entrypoint | 0x80000000) >= MAX_ADDRESS )
		{
			return 0;
		}

		//copy text sections
		for (s8 i = 0; i < 6; i++) 
		{
			if ((!dolfile->sizeText[i]) || (dolfile->addressText[i] < 0x100)) 
				continue;
			_memcpy ((void *) dolfile->addressText[i],binary+dolfile->offsetText[i],dolfile->sizeText[i]);
			DCFlushRangeNoGlobalSync((void *) dolfile->addressText[i], dolfile->sizeText[i]);
			ICInvalidateRange((void *) dolfile->addressText[i],dolfile->sizeText[i]);
		}

		//copy data sections
		for (s8 i = 0; i < 11; i++) 
		{
			if ((!dolfile->sizeData[i]) || (dolfile->addressData[i] < 0x100)) 
				continue;

			set_bss = 
				set_bss && 
				(dolfile->addressData[i]+dolfile->sizeData[i] <= dolfile->addressBSS ||
				dolfile->addressData[i] >= dolfile->addressBSS + dolfile->sizeBSS);

			_memcpy ((void *) dolfile->addressData[i],binary+dolfile->offsetData[i],dolfile->sizeData[i]);
			DCFlushRangeNoGlobalSync((void *) dolfile->addressData[i],dolfile->sizeData[i]);
		}

		//clear BSS - this is the area containing variables. it is required to clear it so we don't have unexpected results
		//cleared before copying the sections kills SM as it has its BSS in the middle of its data sections (nice!)
		//however, not clearing it might cause issues with loader homebrew and their high entrypoints 
		if( set_bss )
		{
			//BSS is in mem2 which means its better to reload ios & then load app (tantric's words)
			/*currently unused cause this is done for wiimc. however reloading ios also loses ahbprot/dvd access...
			if( dolfile->addressBSS >= 0x90000000 )
			{
				//place IOS reload here if it would be needed. the application should've reloaded IOS before us.
			}*/
			_memset ((void *) dolfile->addressBSS, 0, dolfile->sizeBSS);
			DCFlushRangeNoGlobalSync((void *) dolfile->addressBSS, dolfile->sizeBSS);
		}

		//copy over arguments, but only if the dol is meant to have them
		//devkitpro dol's have a magic word set & room in them to have the argument struct copied over them
		//some others, not so much (like some bad compressed dols)
		if (
			( args != NULL && args->argvMagic == ARGV_MAGIC) && //our arguments are valid
			( *(vu32*)(dolfile->entrypoint + 4 | 0x80000000) == ARGV_MAGIC ) //dol supports them too
			)
        {
			void* new_argv = (void*)(dolfile->entrypoint + 8);
			_memcpy(new_argv, args, sizeof(struct __argv));
			DCFlushRangeNoGlobalSync(new_argv, sizeof(struct __argv));
        }
		return(dolfile->entrypoint | 0x80000000);
	}
	return 0;
}

//copy of libogc & gcc, this is to have the loader as small as possible
//this is very bad practice , but this source is meant to be copied to memory and ran as stand alone code
//to be able to do this, and be asured the first code is _start, this code has a copy of the required functions
void _memcpy(void* dst, void* src, u32 len)
{
	u8 *d = dst;
	const u8 *s = src;
	while (len--)
		*d++ = *s++;
	return;
}
void _memset(void* dst, u32 data, u32 len)
{
	u8 *ptr = dst;
	while (len-- > 0)
		*ptr++ = data;
	return;
}

asm(".globl DCFlushRangeNoGlobalSync\n"
	"DCFlushRangeNoGlobalSync:\n"

	//zero or negative size?
	"cmplwi 4, 0\n"
	"blelr\n"

	//check for lower bits set in address
	"clrlwi. 5, 3, 27\n"
	"beq 1f\n"
	"addi 4, 4, 0x20 \n"

	"1:\n"
	"addi 4, 4, 0x1f\n"
	"srwi 4, 4, 5\n"
	"mtctr 4\n"

	//loop
	"2: \n"
	"dcbf 0, 3\n"
	"addi 3, 3, 0x20\n"
	"bdnz 2b\n"

	//sync locally and return
	"sync\n"
	"isync\n"
	"blr\n"
);

asm(".globl ICInvalidateRange\n"
	"ICInvalidateRange:\n"

	//zero or negative size?
	"cmplwi 4, 0\n"
	"blelr\n"

	//check for lower bits set in address
	"clrlwi. 5, 3, 27\n"
	"beq 1f\n"
	"addi 4, 4, 0x20 \n"

	"1:\n"
	"addi 4, 4, 0x1f\n"
	"srwi 4, 4, 5\n"
	"mtctr 4\n"

	//loop
	"2: \n"
	"icbi 0, 3\n"
	"addi 3, 3, 0x20\n"
	"bdnz 2b\n"

	//sync locally and return
	"sync\n"
	"isync\n"
	"blr\n"
);