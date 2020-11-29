/*

priiloader(preloader mod) - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2013-2019  DacoTaco

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
//global functions,defines & variables for priiloader
#include "Global.h"

GXRModeObj *rmode = NULL;
void *xfb = NULL;
s8 Mounted = 0;
s8 Device_Not_Mountable = 0;
static u8 vid_init = 0;
static u8 UsbOnly = 0;

void InitVideo ( void )
{
	if (vid_init == 1)
		return;
	VIDEO_Init();

	if(rmode == NULL)
		rmode = VIDEO_GetPreferredMode(NULL);

	//apparently the video likes to be bigger then it actually is on NTSC/PAL60/480p. lets fix that!
	if( rmode->viTVMode == VI_NTSC || CONF_GetEuRGB60() || CONF_GetProgressiveScan() )
	{
		//the correct one would be * 0.035 to be sure to get on the Action safe of the screen.
		GX_AdjustForOverscan(rmode, rmode, 0, rmode->viWidth * 0.026 ); 
	}

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	console_init( xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth*VI_DISPLAY_PIX_SZ );

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();

	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	vid_init = 1;
	gdprintf("resolution is %dx%d",rmode->viWidth,rmode->viHeight);
	return;
}
s8 GetMountedValue(void)
{
	return Mounted;
}
s8 InitNetwork()
{
	s32 result;
	gprintf("InitNetwork : Waiting for network to initialise...");
    while ((result = net_init()) == -EAGAIN);
    if (result >= 0) 
	{
        char myIP[16];
		//s32 if_config( char *local_ip, char *netmask, char *gateway,bool use_dhcp, int max_retries);
		if (if_config(myIP, NULL, NULL, true,1) < 0) 
		{
			gdprintf("InitNetwork : Error reading IP address");
			return -1;
		}
		gdprintf("InitNetwork : IP Address -> %s",myIP);
		return 1;
    } 
	else 
	{
		gprintf("InitNetwork : DHCP timeout or no known access point availble");
		return -2;
    }
}
u8 GetUsbOnlyMode()
{
	return UsbOnly;
}
u8 ToggleUSBOnlyMode()
{
	if(!UsbOnly && ( __io_usbstorage.startup() ) && ( __io_usbstorage.isInserted() ) && !(Device_Not_Mountable & 1) )
	{
		UsbOnly = 1;
		if(Mounted & 2)
		{
			fatUnmount("fat:");
			Mounted = 0;
		}
		PollDevices();
		
	}
	else
	{
		UsbOnly = 0;
		if(Mounted)
		{
			fatUnmount("fat:");
			Mounted = 0;
		}
		PollDevices();
	}
	return UsbOnly;

}
bool PollDevices( void )
{
	//check mounted device's status and unmount or mount if needed. once something is unmounted, lets see if we can mount something in its place
	if( ( (Mounted & 2) && !__io_wiisd.isInserted() ) || ( (Mounted & 1) && !__io_usbstorage.isInserted() ) )
	{
		fatUnmount("fat:");
		if(Mounted & 1)
		{
			Mounted = 0;
			gprintf("USB removed");
		}
		if(Mounted & 2)
		{
			Mounted = 0;
			gprintf("SD removed");
			__io_wiisd.shutdown();
		}			
	}
	//check if SD is mountable
	if( UsbOnly == 0 && !(Mounted & 2) && __io_wiisd.startup() &&  __io_wiisd.isInserted() && !(Device_Not_Mountable & 2) )
	{
		if(Mounted & 1)
		{
			//USB is mounted. lets kick it out and use SD instead :P
			fatUnmount("fat:");
			Mounted = 0;
			gprintf("USB: Unmounted");
		}
		if(fatMountSimple("fat",&__io_wiisd))
		{
			Mounted |= 2;
			gprintf("SD: Mounted");
		}
		else
		{
			Device_Not_Mountable |= 2;
			gprintf("SD: Failed to mount");
		}
	}
	//check if USB is mountable.deu to short circuit evaluation you need to be VERY CAREFUL when changing the next if or anything usbstorage related
	//i know its stupid to init the USB device and yet not mount it, but thats the only way with c++ & the current usbstorage combo
	//see http://en.wikipedia.org/wiki/Short-circuit_evaluation
	//else if( !(Device_Not_Mountable & 1) && ( __io_usbstorage.startup() ) && ( __io_usbstorage.isInserted() ) && (Mounted == 0) )
	else if( ( __io_usbstorage.startup() ) && ( __io_usbstorage.isInserted() ) && (Mounted == 0) && !(Device_Not_Mountable & 1) )
	{
		//if( fatMountSimple("fat", &__io_usbstorage) )
		if( fatMount("fat", &__io_usbstorage,0, 8, 64) )
		{
			Mounted |= 1;
			gprintf("USB: Mounted");
		}
		else
		{
			Device_Not_Mountable |= 1;
			gprintf("USB: Failed to mount");
		}
	}
	if ( Device_Not_Mountable > 0 )
	{
		if ( ( Device_Not_Mountable & 1 ) && !__io_usbstorage.isInserted() )
		{
			Device_Not_Mountable -= 1;
			gdprintf("USB: NM Flag Reset");
		}
		if ( ( Device_Not_Mountable & 2 ) &&  !__io_wiisd.isInserted() )
		{
			//not needed for SD yet but just to be on the safe side
			Device_Not_Mountable -= 2;
			gdprintf("SD: NM Flag Reset");
			__io_wiisd.shutdown();
		}
	}
	if ( Mounted > 0)
		return true;
	else
		return false;
}
void ShutdownDevices()
{
	//unmount device
	if(Mounted != 0)
	{
		fatUnmount("fat:/");
		Mounted = 0;
	}
	__io_usbstorage.shutdown();
	__io_wiisd.shutdown();	
	return;
}
bool RemountDevices( void )
{
	ShutdownDevices();
	return PollDevices();
}
