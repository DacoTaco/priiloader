#ifndef _HBC_H_
#define _HBC_H_

#include "titles.h"

//HBC reload stub functions
void LoadHBCStub ( void );
void UnloadHBCStub( void );

//HBC Functions
s32 DetectHBC(title_info* title);
void LoadHBC( void );
void LoadBootMii( void );

#endif