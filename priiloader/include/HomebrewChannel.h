#ifndef _HBC_H_
#define _HBC_H_

//HBC reload stub functions
void LoadHBCStub ( void );
void UnloadHBCStub( void );

//HBC Functions
u8 DetectHBC( void );
void LoadHBC( void );
void LoadBootMii( void );

#endif