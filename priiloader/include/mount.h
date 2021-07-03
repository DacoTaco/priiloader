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

#ifndef _MOUNT_H_
#define _MOUNT_H_

#include <string>

//typedef's
typedef void (*mountChangedCallback)(bool sd_mounted, bool usb_mounted);
enum MountDevice {
	Device_Auto = 0,
	Device_SD = 1,
	Device_USB = 2,
};

//functions
#ifdef __cplusplus
extern "C" {
#endif

#define HAS_SD_FLAG(x)		(x & Device_SD)
#define HAS_USB_FLAG(x)		(x & Device_USB)

void InitMounts(mountChangedCallback callback = NULL);
void ShutdownMounts();
std::string BuildPath(const char* path, MountDevice forceDevice = MountDevice::Device_Auto);
void PollMount(void);
u8 GetMountedFlags();

#ifdef __cplusplus
}
#endif
#endif