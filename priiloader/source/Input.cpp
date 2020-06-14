/*

priiloader - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2019 DacoTaco

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

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <wiikeyboard/usbkeyboard.h>
#include <sys/time.h>
#include <sys/unistd.h>

#include "Input.h"
#include "gecko.h"

s8 _input_init = 0;
u32 _STM_input_pressed = 0;
u32 _kbd_pressed = 0;
struct timeval _last_press;
struct timeval _time_init;
static lwp_t kbd_handle = LWP_THREAD_NULL;
static volatile bool kbd_should_quit = false;

void KBEventHandler(USBKeyboard_event event) 
{
	//gprintf("KB event %d", event.type);
	if(event.type != USBKEYBOARD_PRESSED && event.type != USBKEYBOARD_RELEASED)
		return;

	//gprintf("keyCode 0x%X", event.keyCode);
	u32 button = 0;

	switch (event.keyCode) {
		case 0x52: //Up
			button = INPUT_BUTTON_UP;
			break;
		case 0x51: //Down
			button = INPUT_BUTTON_DOWN;
			break;
		case 0x50: //Left
			button = INPUT_BUTTON_LEFT;
			break;
		case 0x4F: //Right
			button = INPUT_BUTTON_RIGHT;
			break;
		case 0x28: //Enter
			button = INPUT_BUTTON_A;
			break;
		case 0x29: //Esc
			button = INPUT_BUTTON_B;
			break;
		case 0x1B: //X
			button = INPUT_BUTTON_X;
			break;
		case 0x1C: //Y
			button = INPUT_BUTTON_Y;
			break;
		case 0x2C: //Space
			button = INPUT_BUTTON_START;
			break;
		default:
			break;
	}

	if(event.type == USBKEYBOARD_PRESSED)
		_kbd_pressed |= button;
	else
		_kbd_pressed &= ~button;
}

void *kbd_thread (void *arg) 
{
	while (!kbd_should_quit) 
	{
		usleep(2000);
		if(!USBKeyboard_IsConnected())
			USBKeyboard_Open(KBEventHandler);

		USBKeyboard_Scan();
	}
	return NULL;
}

//the STM event is triggered when the buttons are pressed, or released.
void HandleSTMEvent(u32 event)
{
	struct timeval press;
	gettimeofday(&press, NULL);	

	//after the 1 second of being init, we wont accept input
	if(_time_init.tv_sec+1 >= press.tv_sec)
		return;

	//only accept input every +/- 200ms
	//this stops it from detecting one press as multiple fires
	if(press.tv_sec == _last_press.tv_sec && (press.tv_usec - _last_press.tv_usec < 200000))
		return;

	gettimeofday(&_last_press, NULL);
	switch(event)
	{
		case STM_EVENT_POWER:
			_STM_input_pressed = INPUT_BUTTON_DOWN;
			break;
		case STM_EVENT_RESET:
			//only send press when the button is pressed, not unpressed
			_STM_input_pressed = (RESET_UNPRESSED == 0)?INPUT_BUTTON_A:0;
			break;
		default:
			_STM_input_pressed = 0;
			break;
	}
	return;
}
s8 Input_Init( void ) 
{
	if(_input_init != 0)
		return 1;

	s8 r = PAD_Init();
	r |= WPAD_Init();
	gettimeofday(&_time_init, NULL);

	STM_RegisterEventHandler(HandleSTMEvent);
	r |= USB_Initialize();
	r |= USBKeyboard_Initialize();	

	// Even if the thread fails to start, 
	kbd_should_quit = false;
	LWP_CreateThread(	&kbd_handle,
							kbd_thread,
							NULL,
							NULL,
							16*1024,
							50);

	_input_init = 1;
	return r;
}
void Input_Shutdown( void )
{
	if (_input_init != 1)
		return;
	
	for (int i = 0;i < WPAD_MAX_WIIMOTES ;i++)
	{
		if(WPAD_Probe(i,0) < 0)
			continue;
		WPAD_Flush(i);
		WPAD_Disconnect(i);
	}

	WPAD_Shutdown();
	kbd_should_quit = true;

	//once libogc is fixed, this should be uncommented and the next keyboard deninit deleted
	//USBKeyboard_Close();
	//USBKeyboard_Deinitialize();	

	if (kbd_handle != LWP_THREAD_NULL) {
		LWP_JoinThread(kbd_handle, NULL);
		kbd_handle = LWP_THREAD_NULL;
	}
	
	USBKeyboard_Close();
	USBKeyboard_Deinitialize();	

	_input_init = 0;
}
u32 Input_ScanPads( void )
{
	WPAD_ScanPads();
	PAD_ScanPads();
	usleep(1000); // Give the keyboard thread a chance to run
	return 1;
}
u32 Input_ButtonsDown( bool _overrideSTM )
{
	u32 pressed = 0;

	if(_STM_input_pressed != 0)
	{
		if(_overrideSTM)
			pressed = INPUT_BUTTON_B;
		else
			pressed = _STM_input_pressed | INPUT_BUTTON_STM;
		_STM_input_pressed = 0;
	}
	else if (_kbd_pressed != 0)
	{
		pressed |= _kbd_pressed;
		_kbd_pressed = 0;
	}
	else
	{
		pressed |= (PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3));
		
		u32 WPAD_Pressed = (WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3));
		
		if( WPAD_Pressed & WPAD_BUTTON_DOWN || WPAD_Pressed & WPAD_CLASSIC_BUTTON_DOWN )
			pressed |= INPUT_BUTTON_DOWN;
		if( WPAD_Pressed & WPAD_BUTTON_UP || WPAD_Pressed & WPAD_CLASSIC_BUTTON_UP )
			pressed |= INPUT_BUTTON_UP;
		if( WPAD_Pressed & WPAD_BUTTON_LEFT || WPAD_Pressed & WPAD_CLASSIC_BUTTON_LEFT )
			pressed |= INPUT_BUTTON_LEFT;
		if( WPAD_Pressed & WPAD_BUTTON_RIGHT || WPAD_Pressed & WPAD_CLASSIC_BUTTON_RIGHT )
			pressed |= INPUT_BUTTON_RIGHT;
		if(WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A)
			pressed |= INPUT_BUTTON_A;
		if(WPAD_Pressed & WPAD_BUTTON_B || WPAD_Pressed & WPAD_CLASSIC_BUTTON_B)
			pressed |= INPUT_BUTTON_B;
		if(WPAD_Pressed & WPAD_BUTTON_PLUS || WPAD_Pressed & WPAD_CLASSIC_BUTTON_PLUS)
			pressed |= INPUT_BUTTON_START;
		if(WPAD_Pressed & WPAD_BUTTON_1 || WPAD_Pressed & WPAD_CLASSIC_BUTTON_Y)
			pressed |= INPUT_BUTTON_Y;
		if(WPAD_Pressed & WPAD_BUTTON_2 || WPAD_Pressed & WPAD_CLASSIC_BUTTON_X)
			pressed |= INPUT_BUTTON_X;
	}

	return pressed;
}
u32 Input_ButtonsDown( void )
{
	return Input_ButtonsDown(false);
}
