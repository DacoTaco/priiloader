/****************************************************************************
 * WiiMC
 * Tantric 2013-2010
 *
 * mem2_manager.c
 *
 * MEM2 allocator

 ****************************************************************************/
#include <ogc/machine/asm.h>
#include <ogc/lwp_heap.h>
#include <ogc/system.h>
#include <ogc/machine/processor.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include "mem2_manager.h"

//1: critical errors     2: detailed info
//#define DEBUG_MEM2_LEVEL 1  // to get info about used mem, it's an approximation because of memory fragmentation

#ifdef DEBUG_MEM2_LEVEL
#include <stdio.h>
#endif
/*** from libogc (lwp_heap.inl) ****/

static __inline__ heap_block* __lwp_heap_blockat(heap_block *block,u32 offset)
{
	return (heap_block*)((char*)block + offset);
}

static __inline__ heap_block* __lwp_heap_usrblockat(void *ptr)
{
	u32 offset = *(((u32*)ptr)-1);
	return __lwp_heap_blockat(ptr,-offset+-HEAP_BLOCK_USED_OVERHEAD);
}
static __inline__ bool __lwp_heap_blockin(heap_cntrl *heap,heap_block *block)
{
	return ((u32)block>=(u32)heap->start && (u32)block<=(u32)heap->final);
}
static __inline__ bool __lwp_heap_blockfree(heap_block *block)
{
	return !(block->front_flag&HEAP_BLOCK_USED);
}
static __inline__ u32 __lwp_heap_blocksize(heap_block *block)
{
	return (block->front_flag&~HEAP_BLOCK_USED);
}

/*** end from libogc (lwp_heap.inl)  ****/

u32 __lwp_heap_block_size(heap_cntrl *theheap,void *ptr)
{
	heap_block *block;
	u32 dsize, level;

	_CPU_ISR_Disable(level);

	block = __lwp_heap_usrblockat(ptr);
	if(!__lwp_heap_blockin(theheap,block) || __lwp_heap_blockfree(block)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	dsize = __lwp_heap_blocksize(block);
	_CPU_ISR_Restore(level);
	return dsize;
}


typedef struct {
	heap_cntrl heap; 
	void *heap_ptr;
 	u32 size;
 	void *old_arena2hi;
 #ifdef DEBUG_MEM2_LEVEL
	u32 allocated;
	u32 top_allocated;
#endif
} st_mem2_area;

static bool _inited=false;

static st_mem2_area mem2_areas[MAX_AREA];

#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)

static void initMem2Areas()
{
	int i;

	for(i = 0; i < MAX_AREA; i++)
	{
		mem2_areas[i].heap_ptr = NULL;
 		mem2_areas[i].size = 0; 
 		mem2_areas[i].old_arena2hi = NULL;
		mem2_areas[i].old_arena2hi = NULL;
#ifdef DEBUG_MEM2_LEVEL		
		mem2_areas[i].allocated = 0;
		mem2_areas[i].top_allocated = 0;
#endif
	}
	_inited=true;
}
 
void ClearMem2Area (const int area) 
{
	if(area >= MAX_AREA)
		return;

	if(mem2_areas[area].size==0) return; // area not found
	memset (mem2_areas[area].heap_ptr, 0, mem2_areas[area].size);
	__lwp_heap_init(&mem2_areas[area].heap, mem2_areas[area].heap_ptr, mem2_areas[area].size, 32);
}

bool AddMem2Area (u32 size, const int index)  
{
	if(!_inited)
		initMem2Areas();

	if(index >= MAX_AREA || size == 0)
		return false;

	if(mem2_areas[index].size == size)
	{
		//ClearMem2Area(index);
		return true;
	}

	if(mem2_areas[index].size > 0 && !RemoveMem2Area(index))
		return false;

#ifdef DEBUG_MEM2_LEVEL
	//if(DEBUG_MEM2_LEVEL == 2)
		printf("AddMem2Area: %i (%d)\n", index, size);
#endif

	mem2_areas[index].old_arena2hi = SYS_GetArena2Hi();
	mem2_areas[index].heap_ptr = (void *)ROUNDDOWN32(((u32)SYS_GetArena2Hi() - size));
	
	if((u32)mem2_areas[index].heap_ptr < (u32)SYS_GetArena2Lo())
	{
#ifdef DEBUG_MEM2_LEVEL
		printf("not enough mem2: %i\n",index);
#endif		
		return false; // not enough mem2
	}

	SYS_SetArena2Hi(mem2_areas[index].heap_ptr);
	__lwp_heap_init(&mem2_areas[index].heap, mem2_areas[index].heap_ptr, size, 64);
	mem2_areas[index].size = size;
	return true;
}

bool RemoveMem2Area(const int area)
{
	if(area >= MAX_AREA || mem2_areas[area].size == 0)
		return false;

	// a lower area is already inited - we cannot deinit this one yet
	int i;

	for(i=0; i < MAX_AREA; i++)
	{
		if(i == area || mem2_areas[i].old_arena2hi == NULL)
			continue;

		if(mem2_areas[i].old_arena2hi < mem2_areas[area].old_arena2hi)
		{
#ifdef DEBUG_MEM2_LEVEL		
			if(DEBUG_MEM2_LEVEL)
				printf("RemoveMem2Area FAILED: %i\n", area);
#endif				
			return false;
		}
	}

	#ifdef DEBUG_MEM2_LEVEL
	//if(DEBUG_MEM2_LEVEL == 2)
		printf("RemoveMem2Area: %i\n", area);
	#endif

	SYS_SetArena2Hi(mem2_areas[area].old_arena2hi);

	memset(&(mem2_areas[area].heap), 0, sizeof(heap_cntrl));
	mem2_areas[area].heap_ptr = NULL;
	mem2_areas[area].size = 0;
	mem2_areas[area].old_arena2hi = NULL;

#ifdef DEBUG_MEM2_LEVEL		
	mem2_areas[area].allocated = 0;
	mem2_areas[area].top_allocated = 0;
#endif

	return true;
}

 
void* mem2_memalign(u8 align, u32 size, const int area)
{
	void *ptr;
 
	if(mem2_areas[area].size==0) 
	{
#ifdef DEBUG_MEM2_LEVEL
		printf("mem2 malloc error: area %i not found\n",area);
#endif		
		return NULL; // area not found
	}
 
	ptr = __lwp_heap_allocate(&mem2_areas[area].heap, size); 

#ifdef DEBUG_MEM2_LEVEL		
	if(ptr == NULL || (mem2_areas[area].allocated + size > mem2_areas[area].size) ) 
	{
		printf("Error not enough mem in malloc, size: %u  area: %i  allocated: %u	top allocated: %u\n",size,area,mem2_areas[area].allocated,mem2_areas[area].top_allocated);
		return NULL;
	}
#else
	if(ptr == NULL) return NULL;
#endif

#ifdef DEBUG_MEM2_LEVEL		
 		mem2_areas[area].allocated += __lwp_heap_block_size(&mem2_areas[area].heap,ptr);
 	
	if(mem2_areas[area].allocated > mem2_areas[area].top_allocated) mem2_areas[area].top_allocated = mem2_areas[area].allocated;

	if(DEBUG_MEM2_LEVEL == 2)
		printf("mem malloc: %u  area: %i  allocated: %u  top allocated: %u \n",size,area,mem2_areas[area].allocated,mem2_areas[area].top_allocated );
#endif
	return ptr;
} 

void* mem2_malloc(u32 size, const int area)
{
	return mem2_memalign(32, size, area);
}

void mem2_free(void *ptr, const int area)
{ 
	if(mem2_areas[area].size==0)
	{
#ifdef DEBUG_MEM2_LEVEL
		printf("mem2 free error: area %i not found\n",area);
#endif		
		return; // area not found
	}

#ifndef DEBUG_MEM2_LEVEL
	__lwp_heap_free(&mem2_areas[area].heap, ptr);
#else
	u32 size=__lwp_heap_block_size(&mem2_areas[area].heap,ptr); 
	 
	if(size == 0)
	{
		printf("mem2 free error: block not found in area %i\n",area);
		return;
	}
	
	mem2_areas[area].allocated -= size;

	if(DEBUG_MEM2_LEVEL == 2)
 		printf("mem2 free: %u	area: %i  allocated: %u  top allocated: %u\n",size,area,mem2_areas[area].allocated,mem2_areas[area].top_allocated);
 
	__lwp_heap_free(&mem2_areas[area].heap, ptr);
#endif
	
 }

void* mem2_realign(u8 align, void* ptr, u32 newsize, const int area)
{
	void *newptr=NULL;

	if(ptr==NULL) return mem2_memalign(align, newsize, area);
	if(newsize==0)
	{
		mem2_free(ptr,area);
		return NULL;
	}
	
	if(mem2_areas[area].size==0)
	{
#ifdef DEBUG_MEM2_LEVEL
		printf("mem2 free error: area %i not found\n",area);
#endif		
		return NULL; // area not found
	}

	u32 size=__lwp_heap_block_size(&mem2_areas[area].heap,ptr); 		

	if(size>newsize) size=newsize;
	
	newptr = mem2_memalign(align, newsize, area);
	
	if(newptr == NULL) return NULL;
	memset(newptr, 0, newsize);
	memcpy(newptr,ptr,size);
	mem2_free(ptr,area);
	return newptr;
}

void* mem2_realloc(void *ptr, u32 newsize, const int area)
{
	return mem2_realign(32, ptr, newsize, area);
}

void* mem2_calloc(u32 num, u32 size, const int area)
{
	void *ptr = mem2_malloc(num*size,area);
	if( ptr == NULL ) return NULL;
	memset(ptr, 0, num*size);
	return ptr;
}

 char *mem2_strdup(const char *s, const int area)
{
    char *ptr= NULL;
    if(s){
        int len = strlen(s) + 1;
        ptr = mem2_calloc(1, len, area);
        if (ptr)
            memcpy(ptr, s, len);
    }
    return ptr;
}

 char *mem2_strndup(const char *s, size_t n, const int area)
{
    char *ptr= NULL;
    if(s){
        int len = n + 1;
        ptr = mem2_calloc(1, len, area);
        if (ptr)
            memcpy(ptr, s, len);
    }
    return ptr;
}

#ifdef DEBUG_MEM2_LEVEL
static void PrintAreaInfo(int index)
{

	heap_iblock info;

	if(mem2_areas[index].size == 0) return;	

	printf("Area: %i. Allocated: %u. Top Allocated: %u\n",index,mem2_areas[index].allocated,mem2_areas[index].top_allocated);
	__lwp_heap_getinfo(&mem2_areas[index].heap,&info);
	printf("Area: %i. free blocks: %u  free size: %u  used blocks: %u  used_size: %u\n",index, 
			info.free_blocks,info.free_size,info.used_blocks,info.used_size); 
}
#endif

void ShowAreaInfo(const int area) //if area == -1 print all areas info
{
#ifdef DEBUG_MEM2_LEVEL

	int i;

	if(area>=0)
		PrintAreaInfo(area);
	else
		for(i = 0; i < MAX_AREA; i++) PrintAreaInfo(i);
#endif
}


