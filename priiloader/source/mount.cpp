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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>

#include <gccore.h>
#include <ogc/mutex.h>

#include "mount.h"
#include "settings.h"
#include "gecko.h"

using namespace std;

//we have this since libogc does not support std::mutex & std::unique_lock
class MutexLock
{
private:
	mutex_t _mutex;
public:
	explicit MutexLock(mutex_t mutex) : _mutex(mutex) 
	{ 
		LWP_MutexLock(_mutex);
	}
	~MutexLock()
	{
		LWP_MutexUnlock(_mutex);
	}
};

string __mountPoint = "sd";
static mountChangedCallback __pollCallback = NULL;
static u8 _init = 0;
static u8 notMountableFlag = 0;
static bool _sdMounted = false;
static bool _usbMounted = false;
static mutex_t mountPointMutex;
static lwp_t mnt_thread_handle = LWP_THREAD_NULL;
static bool quit_thread = 0;

void* _mountThread(void* args)
{
	while (!quit_thread)
	{
		PollMount();		
		usleep(500);
	}

	return 0;
}
void InitMounts(mountChangedCallback callback)
{
	if(__pollCallback == NULL && callback != NULL)
		__pollCallback = callback;

	if (_init)
		return;

	LWP_MutexInit(&mountPointMutex, false);
	quit_thread = false;
	LWP_CreateThread(&mnt_thread_handle, _mountThread, NULL, NULL, 16 * 1024, 50);
	_init = 1;
}

void ShutdownMounts()
{
	if (!_init)
		return;

	//shutdown the mounting thread
	quit_thread = true;
	if (mnt_thread_handle != LWP_THREAD_NULL) 
		LWP_JoinThread(mnt_thread_handle, NULL);
	mnt_thread_handle = LWP_THREAD_NULL;

	//unmount devices
	if (_sdMounted)
		fatUnmount("sd:/");
	_sdMounted = false;
	__io_wiisd.shutdown();

	if (_usbMounted)
		fatUnmount("usb:/");
	_usbMounted = false;
	__io_usbstorage.shutdown();

	__pollCallback = NULL;
	notMountableFlag = 0;
	LWP_MutexDestroy(mountPointMutex);

	_init = 0;
}

u8 GetMountedFlags()
{
	u8 flags = 0;
	MutexLock mountLock(mountPointMutex);

	if (_sdMounted)
		flags |= StorageDevice::SD;

	if (_usbMounted)
		flags |= StorageDevice::USB;

	return flags;
}

string BuildPath(const char* path, StorageDevice device)
{
	string output;
	string semiColon;
	string backslash;
	string mountPoint;
	u32 inputLen = 0;
	const u32 maxInputLen = (MAXPATHLEN + NAME_MAX) - 10;

	MutexLock mountLock(mountPointMutex);
	if (path == NULL || __mountPoint.length() == 0)
	{
		gprintf("path or mountPoint is null");
		return output;
	}
	
	inputLen = strnlen(path, maxInputLen);
	if (inputLen == 0 || inputLen >= maxInputLen)
		return output;

	//add semicolon or backslash to the path if needed
	semiColon = (path[0] == ':') ? "" : ":";
	backslash = (path[0] == '/' || path[1] == '/') ? "" : "/";
	switch (device)
	{
		case StorageDevice::NAND:
			mountPoint = "";
			semiColon = "";
			break;
		case StorageDevice::USB:
			mountPoint = "usb";
			break;
		case StorageDevice::SD:
			mountPoint = "sd";
			break;
		case StorageDevice::Auto:
		default:
			mountPoint = __mountPoint;
			break;
	}

	// "sd" + ":" + "/" + path
	output = mountPoint + semiColon + backslash + path;
	return output;
}

void PollMount(void)
{
	MutexLock mountLock(mountPointMutex);

	//sd card removed?
	if (_sdMounted && !__io_wiisd.isInserted())
	{
		fatUnmount("sd:");
		gprintf("SD: Unmounted");
		__io_wiisd.shutdown();
		_sdMounted = false;
	}

	//or usb removed?
	if (_usbMounted && !__io_usbstorage.isInserted())
	{
		fatUnmount("usb:");
		_usbMounted = false;
		gprintf("USB: Unmounted");
	}

	//check sd it we can mount it?
	if (!_sdMounted && __io_wiisd.startup() && __io_wiisd.isInserted() && !(notMountableFlag & StorageDevice::SD))
	{
		if (fatMountSimple("sd", &__io_wiisd))
		{
			_sdMounted = true;
			gprintf("SD: Mounted");
		}
		else
		{
			notMountableFlag |= StorageDevice::SD;
			gprintf("SD: Failed to mount");
		}
	}

	//or can we mount usb?
	if (__io_usbstorage.startup() && __io_usbstorage.isInserted() && !_usbMounted && !(notMountableFlag & StorageDevice::USB))
	{
		if (fatMountSimple("usb", &__io_usbstorage))
		{
			_usbMounted = true;
			gprintf("USB: Mounted");
		}
		else
		{
			notMountableFlag |= StorageDevice::USB;
			gprintf("USB: Failed to mount");
		}
	}

	//check not mountable flags
	if (notMountableFlag > 0)
	{
		if ((notMountableFlag & StorageDevice::USB) && !__io_usbstorage.isInserted())
		{
			notMountableFlag &= ~StorageDevice::USB;
			gdprintf("USB: NM Flag Reset");
			__io_usbstorage.shutdown();
		}
		if ((notMountableFlag & StorageDevice::SD) && !__io_wiisd.isInserted())
		{
			//not needed for SD yet but just to be on the safe side
			notMountableFlag &= ~StorageDevice::SD;
			gdprintf("SD: NM Flag Reset");
			__io_wiisd.shutdown();
		}
	}

	string oldMountPoint = __mountPoint;
	
	//verify settings and set mountPoint
	switch (settings->PreferredMountPoint)
	{
		case MOUNT_USB:
			if (_usbMounted && __mountPoint != "usb")
				__mountPoint = "usb";
			break;
		case MOUNT_SD:
		case MOUNT_AUTO:
		default:
			if (_sdMounted && __mountPoint != "sd")
				__mountPoint = "sd";
			else if (!_sdMounted && _usbMounted && __mountPoint != "usb")
				__mountPoint = "usb";
			else if(!_usbMounted && !_sdMounted && __mountPoint != "fat")
				__mountPoint = "fat";
			break;
	}
	
	//something changed, notify client?
	if (oldMountPoint != __mountPoint)
	{
		gprintf("Mount switch to %s", __mountPoint.c_str());
		if(__pollCallback != NULL)
			__pollCallback(_sdMounted, _usbMounted);
	}		
}
