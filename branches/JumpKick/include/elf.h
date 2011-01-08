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

#define EI_NIDENT        16

typedef struct {
	unsigned char	e_ident[EI_NIDENT];
	unsigned short	e_type;
	unsigned short	e_machine;
	unsigned int	e_version;
	unsigned int	e_entry;
	unsigned int	e_phoff;
	unsigned int	e_shoff;
	unsigned int	e_flags;
	unsigned short	e_ehsize;
	unsigned short	e_phentsize;
	unsigned short	e_phnum;
	unsigned short	e_shentsize;
	unsigned short	e_shnum;
	unsigned short	e_shstrndx;
 } __attribute__((packed)) Elf32_Ehdr;

typedef struct {
	unsigned int	sh_name;
	unsigned int	sh_type;
	unsigned int	sh_flags;
	unsigned int	sh_addr;
	unsigned int	sh_offset;
	unsigned int	sh_size;
	unsigned int	sh_link;
	unsigned int	sh_info;
	unsigned int	sh_addralign;
	unsigned int	sh_entsize;
} __attribute__((packed)) Elf32_Shdr;

typedef struct {
	unsigned int	p_type;
	unsigned int	p_offset;
	unsigned int	p_vaddr;
	unsigned int	p_paddr;
	unsigned int	p_filesz;
	unsigned int	p_memsz;
	unsigned int	p_flags;
	unsigned int	p_align;
} __attribute__((packed)) Elf32_Phdr;

#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_PAD		7
#define EI_NIDENT	16	//size of ident

/* Segment types - p_type */
#define PT_NULL         0               /* unused */
#define PT_LOAD         1               /* loadable segment */
#define PT_DYNAMIC      2               /* dynamic linking section */
#define PT_INTERP       3               /* the RTLD */
#define PT_NOTE         4               /* auxiliary information */
#define PT_SHLIB        5               /* reserved - purpose undefined */
#define PT_PHDR         6               /* program header */
#define PT_TLS          7               /* Thread local storage template */
#define PT_NUM          8               /* Number of segment types */
#define PT_LOOS         0x60000000      /* reserved range for operating */
#define PT_HIOS         0x6fffffff      /*   system specific segment types */
#define PT_LOPROC       0x70000000      /* reserved range for processor */
#define PT_HIPROC       0x7fffffff      /*  specific segment types */

/* Segment flags - p_flags */
#define PF_X            0x1             /* Executable */
#define PF_W            0x2             /* Writable */
#define PF_R            0x4             /* Readable */
#define PF_MASKOS       0x0ff00000      /* OS specific segment flags */
#define PF_MASKPROC     0xf0000000      /* reserved bits for processor */

/* sh_type */
#define SHT_NULL        0               /* inactive */
#define SHT_PROGBITS    1               /* program defined information */
#define SHT_SYMTAB      2               /* symbol table section */
#define SHT_STRTAB      3               /* string table section */
#define SHT_RELA        4               /* relocation section with addends*/
#define SHT_HASH        5               /* symbol hash table section */
#define SHT_DYNAMIC     6               /* dynamic section */
#define SHT_NOTE        7               /* note section */
#define SHT_NOBITS      8               /* no space section */
#define SHT_REL         9               /* relation section without addends */
#define SHT_SHLIB       10              /* reserved - purpose unknown */
#define SHT_DYNSYM      11              /* dynamic symbol table section */
#define SHT_INIT_ARRAY  14              /* Array of constructors */
#define SHT_FINI_ARRAY  15              /* Array of destructors */
#define SHT_PREINIT_ARRAY 16            /* Array of pre-constructors */
#define SHT_GROUP       17              /* Section group */
#define SHT_SYMTAB_SHNDX 18             /* Extended section indeces */
#define SHT_NUM         19              /* number of section types */
#define SHT_LOOS        0x60000000      /* Start OS-specific */
#define SHT_HIOS        0x6fffffff      /* End OS-specific */
#define SHT_LOPROC      0x70000000      /* reserved range for processor */
#define SHT_HIPROC      0x7fffffff      /*  specific section header types */
#define SHT_LOUSER      0x80000000      /* reserved range for application */
#define SHT_HIUSER      0xffffffff      /*  specific indexes */
