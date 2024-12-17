/****************************************************************************
 * WiiMC
 * Tantric 2013-2010
 *
 * mem2_manager.h
 *
 * MEM2 allocator
 ****************************************************************************/

#ifndef _MEM2MANAGER_H_
#define _MEM2MANAGER_H_
#include <ogc/system.h>

#define ALIGN32(x) (((x) + 31) & ~31)
#define mem_malloc(size) mem2_malloc(ALIGN32(size),OTHER_AREA)
#define mem_free(x) if(x != NULL) { mem2_free(x,OTHER_AREA);x=NULL; }
#define mem_align(align,size) mem2_memalign(align,ALIGN32(size),OTHER_AREA)
#define mem_realloc(oldPtr, newSize) mem2_realloc(oldPtr, ALIGN32(newSize), OTHER_AREA)
#define mem_realign(align, oldPtr, newSize) mem2_realign(align, oldPtr, ALIGN32(newSize), OTHER_AREA);

#ifndef ATTRIBUTE_ALIGN
#define ATTRIBUTE_ALIGN(v)							[[gnu::aligned(v)]]
#endif

#ifndef STACK_ALIGN
// courtesy of Marcan
#define STACK_ALIGN(type, name, cnt, alignment)		u8 _al__##name[((sizeof(type)*(cnt)) + (alignment) + (((sizeof(type)*(cnt))%(alignment)) > 0 ? ((alignment) - ((sizeof(type)*(cnt))%(alignment))) : 0))]; \
													type *name = (type*)(((u32)(_al__##name)) + ((alignment) - (((u32)(_al__##name))&((alignment)-1))))
#endif

enum mem2_areas_enum {
	VIDEO_AREA,
	GUI_AREA,
	OTHER_AREA,
	PICTURE_AREA,
	INDEX_AREA,
	MAX_AREA
};
 
#ifdef __cplusplus
extern "C" {
#endif
 
bool AddMem2Area (u32 size, const int index);
bool RemoveMem2Area(const int area);
void ClearMem2Area (const int area);
void* mem2_memalign(u8 align, u32 size, const int area);
void* mem2_malloc(u32 size, const int area);
void mem2_free(void *ptr, const int area);	
void* mem2_calloc(u32 num, u32 size, const int area);
void* mem2_realloc(void *ptr, u32 newsize, const int area);
void* mem2_realign(u8 align, void* ptr, u32 newsize, const int area);
char *mem2_strdup(const char *s, const int area);
char *mem2_strndup(const char *s, size_t n, const int area);

void ShowAreaInfo(const int area); //if area == -1 print all areas info

#ifdef __cplusplus
}
#endif

#endif
