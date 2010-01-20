#ifndef _ELF_H_
#define _ELF_H_

#include <gctypes.h>

s32 valid_elf_image (void *addr);
u32 load_elf_image (void *addr);

#endif
