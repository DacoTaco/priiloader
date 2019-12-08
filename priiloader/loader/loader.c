/*

priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console
Executable Loader - Loads any executable who has been loaded into memory

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

#include <ogc/machine/processor.h>
#include <gctypes.h>
#include "executables.h"

#ifndef MAX_ADDRESS
#define MAX_ADDRESS 0x817FFEFF
#endif

void DCFlushRange(void *startaddress,u32 len);
void ICInvalidateRange(void* addr, u32 size);
void _memcpy(void* dst, void* src, u32 len);
void _memset(void* src, u32 data, u32 len);
void startSysMenu(void);
u32 _loadApplication(void* binary, void* parameter);
u32 _loadSystemMenu(void* binary, void* parameter, u32 parameterCount);

//this --MUST-- be the first code in this file.
//when run, we will jump to addr 0 of the compiled code. if this is on top, this will be the code run
void _start(void* binary, void* parameter, u32 parameterCount, u8 isSystemMenu)
{
	/*this would make the loader fully stand alone (stack pointer in mem2)
	and capable of loading any dol in mem1
	however, this somehow breaks loading system menu (and dols/elfs)
	if i can figure out why, i'll need to rewrite _start in asm and 
	1) load the arguments (into some registers or something)
	2) change r1/r31 to the new stack address
	3) save arguments to the new stack
	3) do our magic of moving the dol around
	4) ??? (unknown magic atm)
	5) PROFIT !!! (dol loaded)
	this is test code and must under no reason be used!! */

	/*u32 s;
	#define STACK_PTR_PREV *(vu32*)0x93FFFFE0
	asm volatile("lwz %0,0(31) ; sync" : "=r"(s));
	STACK_PTR_PREV = s;
	asm(R"(
		lis		1,0x93F00000@h
		ori		1,1,0x93F00000@l
		stwu 1,-0(1)
		stw 31,44(1)
		mr 31,1)");
	u32 prev_stack = s;
	u32 test = 0xDEAD;
	test = 0xBEEF;
	*(vu32*)0x817FFFF0 = 0x01;*/

	if(binary == NULL || (parameter == NULL && parameterCount > 0))
		return;

	u32 ep = (isSystemMenu)?_loadSystemMenu(binary,parameter,parameterCount):_loadApplication(binary,parameter);
	if(!ep)
		return;

	*(vu32*)0x800000F8 = 0x0E7BE2C0;				// Bus Clock Speed
	*(vu32*)0x800000FC = 0x2B73A840;				// CPU Clock Speed
	*(vu32*)0x8000315C = 0x80800113;				// DI Legacy mode ?

	if(isSystemMenu)
	{
		mtmsr(mfmsr() & ~0x8000);
		mtmsr(mfmsr() | 0x2002);
		startSysMenu();
	}
	else
	{
		void	(*entrypoint)();
		entrypoint = (void (*)())(ep);
		asm("isync");
		entrypoint();
	}
	
	return;
}

u32 _loadApplication(void* binary, void* parameter)
{
	Elf32_Ehdr *ElfHdr = (Elf32_Ehdr *)binary;
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
			Elf32_Phdr* phdr;
			phdr = binary + (ElfHdr->e_phoff + sizeof( Elf32_Phdr ) * i);
			ICInvalidateRange ((void*)(phdr->p_vaddr | 0x80000000),phdr->p_filesz);
			if(phdr->p_type != PT_LOAD )
				continue;
			_memcpy((void*)(phdr->p_vaddr | 0x80000000), binary + phdr->p_offset , phdr->p_filesz);
		}

		//according to dhewg the section headers are totally un-needed (infact, they break a few elf loading)
		//however, checking for the type does the trick to make them work :)
		for( s32 i=0; i < ElfHdr->e_shnum; ++i )
		{
			Elf32_Shdr *shdr;
			shdr = binary + (ElfHdr->e_shoff + sizeof( Elf32_Shdr ) * i);

			//useless check
			//if( shdr->sh_type == SHT_NULL )
			//	continue;

			if (shdr->sh_type != SHT_NOBITS)
				continue;
				
			_memcpy((void*)(shdr->sh_addr | 0x80000000), binary + shdr->sh_offset,shdr->sh_size);
			DCFlushRange((void*)(shdr->sh_addr | 0x80000000),shdr->sh_size);
		}
		return (ElfHdr->e_entry | 0x80000000);	
	}
	else
	{
		dolhdr *dolfile;
		dolfile = (dolhdr *)binary;

		//entrypoint & BSS checking
		if( (dolfile->entrypoint | 0x80000000) < 0x80003400 || (dolfile->entrypoint | 0x80000000) >= MAX_ADDRESS )
		{
			return 0;
		}
		if( dolfile->addressBSS >= 0x90000000 )
		{
			//BSS is in mem2 which means its better to reload ios & then load app. i dont really get it but thats what tantric said
			//currently unused cause this is done for wiimc. however reloading ios also looses ahbprot/dvd access...
			
			//place IOS reload here
		}

		//copy text sections
		for (s8 i = 0; i < 7; i++) {
			if ((!dolfile->sizeText[i]) || (dolfile->addressText[i] < 0x100)) 
				continue;
			_memcpy ((void *) dolfile->addressText[i],binary+dolfile->offsetText[i],dolfile->sizeText[i]);
			DCFlushRange ((void *) dolfile->addressText[i], dolfile->sizeText[i]);
			ICInvalidateRange((void *) dolfile->addressText[i],dolfile->sizeText[i]);
		}

		//copy data sections
		for (s8 i = 0; i < 11; i++) {
			if ((!dolfile->sizeData[i]) || (dolfile->offsetData[i] < 0x100)) 
				continue;
			_memcpy ((void *) dolfile->addressData[i],binary+dolfile->offsetData[i],dolfile->sizeData[i]);
			DCFlushRange((void *) dolfile->offsetData[i],dolfile->sizeData[i]);
		}

		if( 
			( dolfile->addressBSS + dolfile->sizeBSS < 0x80F00000 ||(dolfile->addressBSS > 0x81500000 && dolfile->addressBSS + dolfile->sizeBSS < MAX_ADDRESS) ) &&
			dolfile->addressBSS > 0x80003400 )
		{
			_memset ((void *) dolfile->addressBSS, 0, dolfile->sizeBSS);
			DCFlushRange((void *) dolfile->addressBSS, dolfile->sizeBSS);
		}

		//copy over arguments, but only if the dol is meant to have them
		//devkitpro dol's have room in them to have the argument struct copied over them
		//others, not so much (like compressed dols)
		if (*(vu32*)(dolfile->entrypoint + 8 | 0x80000000) == 0x00 && args != NULL && args->argvMagic == ARGV_MAGIC)
        {
			void* new_argv = (void*)(dolfile->entrypoint + 8);
			_memcpy(new_argv, args, sizeof(struct __argv));
			DCFlushRange(new_argv, sizeof(struct __argv));
        }
		return(dolfile->entrypoint | 0x80000000);
	}
	return 0;
}

u32 _loadSystemMenu(void* binary, void* parameter, u32 parameterCount)
{	
	u32 entrypoint = _loadApplication(binary,NULL);

	if(entrypoint != 0x80003400)
		return 0;

	/* offset patches will go here*/

	return entrypoint;
}

//unstub asm code by crediar. basically boots the system menu NAND boot code
asm(R"(.globl startSysMenu
startSysMenu:
        isync
#set MSR[DR:IR] = 00, jump to STUB
        lis 3,0x3400@h
        ori 3,3,0x3400@l
        mtsrr0 3

        mfmsr 3
        li 4,0x30
        andc 3,3,4
        mtsrr1 3
        rfi)");

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

asm(R"(.globl DCFlushRange
DCFlushRange:
	cmplwi 4, 0   # zero or negative size?
	blelr
	clrlwi. 5, 3, 27  # check for lower bits set in address
	beq 1f
	addi 4, 4, 0x20 
1:
	addi 4, 4, 0x1f
	srwi 4, 4, 5
	mtctr 4
2:
	dcbf 0, 3
	addi 3, 3, 0x20
	bdnz 2b
	sc
	blr)");

asm(R"(.globl ICInvalidateRange
ICInvalidateRange:
	cmplwi 4, 0   # zero or negative size?
	blelr
	clrlwi. 5, 3, 27  # check for lower bits set in address
	beq 1f
	addi 4, 4, 0x20 
1:
	addi 4, 4, 0x1f
	srwi 4, 4, 5
	mtctr 4
2:
	icbi 0, 3
	addi 3, 3, 0x20
	bdnz 2b
	sync
	isync
	blr)");