/*-------------------------------------------------------------

usbstorage.c -- Bulk-only USB mass storage support

Copyright (C) 2008
Sven Peter (svpe) <svpe@gmx.net>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/
#if defined(HW_RVL)

#include <gccore.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <ogc/lwp_heap.h>
#include <malloc.h>

#include <ogc/machine/asm.h>
#include <ogc/machine/processor.h>
#include <ogc/disc_io.h>

#define ROUNDDOWN32(v)				(((u32)(v) - 0x1f) & ~0x1f)

#define HEAP_SIZE					(32*1024)
#define TAG_START					0x0BADC0DE

#define CBW_SIZE					31
#define CBW_SIGNATURE				0x43425355
#define CBW_IN						(1 << 7)
#define CBW_OUT						0

#define CSW_SIZE					13
#define CSW_SIGNATURE				0x53425355

#define SCSI_TEST_UNIT_READY		0x00
#define SCSI_REQUEST_SENSE			0x03
#define SCSI_READ_CAPACITY			0x25
#define SCSI_READ_10				0x28
#define SCSI_WRITE_10				0x2A

#define SCSI_SENSE_REPLY_SIZE		18
#define SCSI_SENSE_NOT_READY		0x02
#define SCSI_SENSE_MEDIUM_ERROR		0x03
#define SCSI_SENSE_HARDWARE_ERROR	0x04

#define USB_CLASS_MASS_STORAGE		0x08
#define MASS_STORAGE_SCSI_COMMANDS	0x06
#define MASS_STORAGE_BULK_ONLY		0x50

#define USBSTORAGE_GET_MAX_LUN		0xFE
#define USBSTORAGE_RESET			0xFF

#define USB_ENDPOINT_BULK			0x02

#define USBSTORAGE_CYCLE_RETRIES	3

#define MAX_TRANSFER_SIZE_V0		(4 * 1024)
#define MAX_TRANSFER_SIZE_V5		(16 * 1024)

#define DEVLIST_MAXSIZE				8

static heap_cntrl __heap;
static bool __inited = false;
static lwpq_t __usbstorage_waitq = 0;

/*
The following is for implementing a DISC_INTERFACE
as used by libfat
*/

static usbstorage_handle __usbfd;
static u8 __lun = 0;
static bool __mounted = false;
static bool __removed = false;
static u16 __vid = 0;
static u16 __pid = 0;

static s32 __usbstorage_reset(usbstorage_handle *dev);
static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun);

/* XXX: this is a *really* dirty and ugly way to send a bulkmessage with a timeout
 *      but there's currently no other known way of doing this and it's in my humble
 *      opinion still better than having a function blocking forever while waiting
 *      for the USB data/IOS reply..
 */

static s32 __usb_blkmsg_cb(s32 retval, void *dummy)
{
	usbstorage_handle *dev = (usbstorage_handle *)dummy;
	dev->retval = retval;
	SYS_CancelAlarm(dev->alarm);
	LWP_ThreadBroadcast(__usbstorage_waitq);
	return 0;
}

static s32 __usb_deviceremoved_cb(s32 retval,void *arg)
{
	__mounted = false;
	__removed = true;
	return 0;
}

static void __usb_timeouthandler(syswd_t alarm,void *cbarg)
{
	usbstorage_handle *dev = (usbstorage_handle*)cbarg;
	dev->retval = USBSTORAGE_ETIMEDOUT;
	LWP_ThreadBroadcast(__usbstorage_waitq);
}

static void __usb_settimeout(usbstorage_handle *dev)
{
	struct timespec ts;

	ts.tv_sec = 2;
	ts.tv_nsec = 0;
	SYS_SetAlarm(dev->alarm,&ts,__usb_timeouthandler,dev);
}

static s32 __USB_BlkMsgTimeout(usbstorage_handle *dev, u8 bEndpoint, u16 wLength, void *rpData)
{
	u32 level;
	s32 retval;

	dev->retval = USBSTORAGE_PROCESSING;
	retval = USB_WriteBlkMsgAsync(dev->usb_fd, bEndpoint, wLength, rpData, __usb_blkmsg_cb, (void *)dev);
	if(retval < 0) return retval;

	__usb_settimeout(dev);

	_CPU_ISR_Disable(level);
	do {
		retval = dev->retval;
		if(retval!=USBSTORAGE_PROCESSING) break;
		else LWP_ThreadSleep(__usbstorage_waitq);
	} while(retval==USBSTORAGE_PROCESSING);
	_CPU_ISR_Restore(level);

	if (retval<0)
		USB_ClearHalt(dev->usb_fd, bEndpoint);

	return retval;
}

static s32 __USB_CtrlMsgTimeout(usbstorage_handle *dev, u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, u16 wLength, void *rpData)
{
	u32 level;
	s32 retval;
	struct timespec ts;

	ts.tv_sec = 2;
	ts.tv_nsec = 0;

	dev->retval = USBSTORAGE_PROCESSING;
	retval = USB_WriteCtrlMsgAsync(dev->usb_fd, bmRequestType, bmRequest, wValue, wIndex, wLength, rpData, __usb_blkmsg_cb, (void *)dev);
	if(retval < 0) return retval;

	__usb_settimeout(dev);

	_CPU_ISR_Disable(level);
	do {
		retval = dev->retval;
		if(retval!=USBSTORAGE_PROCESSING) break;
		else LWP_ThreadSleep(__usbstorage_waitq);
	} while(retval==USBSTORAGE_PROCESSING);
	_CPU_ISR_Restore(level);

	return retval;
}

s32 USBStorage_Initialize()
{
	u8 *ptr;
	u32 level;

	if (__inited)
		return IPC_OK;

	_CPU_ISR_Disable(level);

	LWP_InitQueue(&__usbstorage_waitq);

	ptr = (u8*)ROUNDDOWN32(((u32)SYS_GetArena2Hi() - HEAP_SIZE));
	if((u32)ptr < (u32)SYS_GetArena2Lo()) {
		_CPU_ISR_Restore(level);
		return IPC_ENOMEM;
	}

	SYS_SetArena2Hi(ptr);

	__lwp_heap_init(&__heap, ptr, HEAP_SIZE, 32);
	__inited = true;
	_CPU_ISR_Restore(level);

	return IPC_OK;
}

static s32 __send_cbw(usbstorage_handle *dev, u8 lun, u32 len, u8 flags, const u8 *cb, u8 cbLen)
{
	s32 retval = USBSTORAGE_OK;

	if(cbLen == 0 || cbLen > 16)
		return IPC_EINVAL;

	memset(dev->buffer, 0, CBW_SIZE);

	__stwbrx(dev->buffer, 0, CBW_SIGNATURE);
	__stwbrx(dev->buffer, 4, dev->tag);
	__stwbrx(dev->buffer, 8, len);
	dev->buffer[12] = flags;
	dev->buffer[13] = lun;
	dev->buffer[14] = (cbLen > 6 ? 10 : 6);

	memcpy(dev->buffer + 15, cb, cbLen);

	if(dev->suspended == 1)
	{
		USB_ResumeDevice(dev->usb_fd);
		dev->suspended = 0;
	}

	retval = __USB_BlkMsgTimeout(dev, dev->ep_out, CBW_SIZE, (void *)dev->buffer);

	if(retval == CBW_SIZE) return USBSTORAGE_OK;
	else if(retval > 0) return USBSTORAGE_ESHORTWRITE;

	return retval;
}

static s32 __read_csw(usbstorage_handle *dev, u8 *status, u32 *dataResidue)
{
	s32 retval = USBSTORAGE_OK;
	u32 signature, tag, _dataResidue, _status;

	memset(dev->buffer, 0, CSW_SIZE);

	retval = __USB_BlkMsgTimeout(dev, dev->ep_in, CSW_SIZE, dev->buffer);
	if(retval > 0 && retval != CSW_SIZE) return USBSTORAGE_ESHORTREAD;
	else if(retval < 0) return retval;

	signature = __lwbrx(dev->buffer, 0);
	tag = __lwbrx(dev->buffer, 4);
	_dataResidue = __lwbrx(dev->buffer, 8);
	_status = dev->buffer[12];

	if(signature != CSW_SIGNATURE) return USBSTORAGE_ESIGNATURE;

	if(dataResidue != NULL)
		*dataResidue = _dataResidue;
	if(status != NULL)
		*status = _status;

	if(tag != dev->tag) return USBSTORAGE_ETAG;
	dev->tag++;

	return USBSTORAGE_OK;
}

static s32 __cycle(usbstorage_handle *dev, u8 lun, u8 *buffer, u32 len, u8 *cb, u8 cbLen, u8 write, u8 *_status, u32 *_dataResidue)
{
	s32 retval = USBSTORAGE_OK;

	u8 status = 0;
	u32 dataResidue = 0;
	u32 thisLen;
	u16 max_size;

	s8 retries = USBSTORAGE_CYCLE_RETRIES + 1;
	if (dev->usb_fd >= 0 && dev->usb_fd < 0x20)
		max_size = MAX_TRANSFER_SIZE_V0;
	else
		max_size = MAX_TRANSFER_SIZE_V5;

	LWP_MutexLock(dev->lock);
	do
	{
		retries--;

		if(retval == USBSTORAGE_ETIMEDOUT)
			break;

		if(write)
		{
			retval = __send_cbw(dev, lun, len, CBW_OUT, cb, cbLen);
			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
			while(len > 0)
			{
				thisLen = len > max_size ? max_size : len;
				if ((u32)buffer&0x1F || !((u32)buffer&0x10000000)) {
					memcpy(dev->buffer, buffer, thisLen);
					retval = __USB_BlkMsgTimeout(dev, dev->ep_out, thisLen, dev->buffer);
				} else
					retval = __USB_BlkMsgTimeout(dev, dev->ep_out, thisLen, buffer);

				if (retval == USBSTORAGE_ETIMEDOUT)
					break;

				if(retval < 0)
				{
					retval = USBSTORAGE_EDATARESIDUE;
					break;
				}

				if(retval != thisLen && len > 0)
				{
					retval = USBSTORAGE_EDATARESIDUE;
					break;
				}
				len -= retval;
				buffer += retval;
			}

			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
		}
		else
		{
			retval = __send_cbw(dev, lun, len, CBW_IN, cb, cbLen);


			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
			while(len > 0)
			{
				thisLen = len > max_size ? max_size : len;
				if ((u32)buffer&0x1F || !((u32)buffer&0x10000000)) {
					retval = __USB_BlkMsgTimeout(dev, dev->ep_in, thisLen, dev->buffer);
					if (retval>0)
						memcpy(buffer, dev->buffer, retval);
				} else
					retval = __USB_BlkMsgTimeout(dev, dev->ep_in, thisLen, buffer);

				if (retval == USBSTORAGE_ETIMEDOUT || retval != thisLen)
					break;

				len -= retval;
				buffer += retval;
			}

			if(retval < 0)
			{
				if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
					retval = USBSTORAGE_ETIMEDOUT;
				continue;
			}
		}

		retval = __read_csw(dev, &status, &dataResidue);

		if(retval < 0)
		{
			if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
				retval = USBSTORAGE_ETIMEDOUT;
			continue;
		}

		retval = USBSTORAGE_OK;
	} while(retval < 0 && retries > 0);

	if(retval < 0 && retval != USBSTORAGE_ETIMEDOUT)
	{
		if(__usbstorage_reset(dev) == USBSTORAGE_ETIMEDOUT)
			retval = USBSTORAGE_ETIMEDOUT;
	}
	LWP_MutexUnlock(dev->lock);


	if(_status != NULL)
		*_status = status;
	if(_dataResidue != NULL)
		*_dataResidue = dataResidue;

	return retval;
}

static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[16];
	u8 sense[SCSI_SENSE_REPLY_SIZE];
	u8 status = 0;

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_TEST_UNIT_READY;

	retval = __cycle(dev, lun, NULL, 0, cmd, 1, 0, &status, NULL);
	if (retval < 0) return retval;

	if (status)
	{
		cmd[0] = SCSI_REQUEST_SENSE;
		cmd[1] = lun << 5;
		cmd[4] = SCSI_SENSE_REPLY_SIZE;
		cmd[5] = 0;
		memset(sense, 0, SCSI_SENSE_REPLY_SIZE);
		retval = __cycle(dev, lun, sense, SCSI_SENSE_REPLY_SIZE, cmd, 6, 0, NULL, NULL);
		if (retval>=0) {
			switch (sense[2]&0xF) {
				case SCSI_SENSE_NOT_READY:
					return USBSTORAGE_EINIT;
				case SCSI_SENSE_MEDIUM_ERROR:
				case SCSI_SENSE_HARDWARE_ERROR:
					return USBSTORAGE_ESENSE;
			}
		}
	}

	return retval;
}

static s32 __usbstorage_reset(usbstorage_handle *dev)
{
	s32 retval = __USB_CtrlMsgTimeout(dev, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_RESET, 0, dev->interface, 0, NULL);

	if (retval<0 && retval != -7004)
		goto end;

	//  don't clear the endpoints, it makes too many devices die

	//retval = USB_ClearHalt(dev->usb_fd, dev->ep_in);
	//if (retval < 0)	goto end;
	//retval = USB_ClearHalt(dev->usb_fd, dev->ep_out);

	usleep(100);

end:
	return retval;
}

s32 USBStorage_Open(usbstorage_handle *dev, s32 device_id, u16 vid, u16 pid)
{
	s32 retval = -1;
	u8 conf;
	u8 *max_lun;
	u32 iConf, iInterface, iEp;
	usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;

	max_lun = __lwp_heap_allocate(&__heap, 1);
	if (!max_lun)
		return IPC_ENOMEM;

	memset(dev, 0, sizeof(*dev));
	dev->usb_fd = -1;

	dev->tag = TAG_START;

	if (LWP_MutexInit(&dev->lock, false) < 0)
		goto free_and_return;

	if (SYS_CreateAlarm(&dev->alarm) < 0)
		goto free_and_return;

	retval = USB_OpenDevice(device_id, vid, pid, &dev->usb_fd);
	if (retval < 0)
		goto free_and_return;

	retval = USB_GetDescriptors(dev->usb_fd, &udd);
	if (retval < 0)
		goto free_and_return;

	for (iConf = 0; iConf < udd.bNumConfigurations; iConf++) {
		ucd = &udd.configurations[iConf];
		for (iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++) {
			uid = &ucd->interfaces[iInterface];
			if (uid->bInterfaceClass    == USB_CLASS_MASS_STORAGE &&
				uid->bInterfaceSubClass == MASS_STORAGE_SCSI_COMMANDS &&
				uid->bInterfaceProtocol == MASS_STORAGE_BULK_ONLY)
			{
				if (uid->bNumEndpoints < 2)
					continue;

				dev->ep_in = dev->ep_out = 0;
				for (iEp = 0; iEp < uid->bNumEndpoints; iEp++) {
					ued = &uid->endpoints[iEp];
					if (ued->bmAttributes != USB_ENDPOINT_BULK)
						continue;

					if (ued->bEndpointAddress & USB_ENDPOINT_IN)
						dev->ep_in = ued->bEndpointAddress;
					else
						dev->ep_out = ued->bEndpointAddress;
				}

				if (dev->ep_in != 0 && dev->ep_out != 0) {
					dev->configuration = ucd->bConfigurationValue;
					dev->interface = uid->bInterfaceNumber;
					dev->altInterface = uid->bAlternateSetting;
					goto found;
				}
			}
		}
	}

	USB_FreeDescriptors(&udd);
	retval = USBSTORAGE_ENOINTERFACE;
	goto free_and_return;

found:
	USB_FreeDescriptors(&udd);

	retval = USBSTORAGE_EINIT;
	// some devices return an error, ignore it
	USB_GetConfiguration(dev->usb_fd, &conf);

	if (conf != dev->configuration && USB_SetConfiguration(dev->usb_fd, dev->configuration) < 0)
		goto free_and_return;

	if (dev->altInterface != 0 && USB_SetAlternativeInterface(dev->usb_fd, dev->interface, dev->altInterface) < 0)
		goto free_and_return;

	dev->suspended = 0;

	retval = USBStorage_Reset(dev);
	if (retval < 0)
		goto free_and_return;

	LWP_MutexLock(dev->lock);
	retval = __USB_CtrlMsgTimeout(dev, (USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_GET_MAX_LUN, 0, dev->interface, 1, max_lun);
	LWP_MutexUnlock(dev->lock);

	if (retval < 0)
		dev->max_lun = 1;
	else
		dev->max_lun = *max_lun + 1;

	if (retval == USBSTORAGE_ETIMEDOUT)
		goto free_and_return;

	retval = USBSTORAGE_OK;
	dev->sector_size = (u32 *) calloc(dev->max_lun, sizeof(u32));
	if(!dev->sector_size) {
		retval = IPC_ENOMEM;
		goto free_and_return;
	}

	/* taken from linux usbstorage module (drivers/usb/storage/transport.c)
	 *
	 * Some devices (i.e. Iomega Zip100) need this -- apparently
	 * the bulk pipes get STALLed when the GetMaxLUN request is
	 * processed.   This is, in theory, harmless to all other devices
	 * (regardless of if they stall or not).
	 *
	 * 8/9/10: If anyone wants to actually use a Zip100, they can add this back.
	 * But for now, it seems to be breaking things more than it is helping.
	 */
	//USB_ClearHalt(dev->usb_fd, dev->ep_in);
	//USB_ClearHalt(dev->usb_fd, dev->ep_out);

	dev->buffer = __lwp_heap_allocate(&__heap, MAX_TRANSFER_SIZE_V5);

	if(!dev->buffer) {
		retval = IPC_ENOMEM;
	} else {
		USB_DeviceRemovalNotifyAsync(dev->usb_fd,__usb_deviceremoved_cb,dev);
		retval = USBSTORAGE_OK;
	}

free_and_return:
	if (max_lun)
		__lwp_heap_free(&__heap, max_lun);

	if (retval < 0) {
		USBStorage_Close(dev);
		return retval;
	}

	return 0;
}

s32 USBStorage_Close(usbstorage_handle *dev)
{
	__mounted = false;
	__removed = false;
	__lun = 0;
	__vid = 0;
	__pid = 0;

	if (dev->usb_fd != -1)
		USB_CloseDevice(&dev->usb_fd);

	LWP_MutexDestroy(dev->lock);
	SYS_RemoveAlarm(dev->alarm);

	free(dev->sector_size);

	if (dev->buffer)
		__lwp_heap_free(&__heap, dev->buffer);

	memset(dev, 0, sizeof(*dev));
	dev->usb_fd = -1;
	return 0;
}

s32 USBStorage_Reset(usbstorage_handle *dev)
{
	s32 retval;

	LWP_MutexLock(dev->lock);
	retval = __usbstorage_reset(dev);
	LWP_MutexUnlock(dev->lock);

	return retval;
}

s32 USBStorage_GetMaxLUN(usbstorage_handle *dev)
{
	return dev->max_lun;
}

s32 USBStorage_MountLUN(usbstorage_handle *dev, u8 lun)
{
	s32 retval;

	if(lun >= dev->max_lun)
		return IPC_EINVAL;

	retval = __usbstorage_clearerrors(dev, lun);
	if (retval<0)
		return retval;

	retval = USBStorage_ReadCapacity(dev, lun, &dev->sector_size[lun], NULL);
	return retval;
}

s32 USBStorage_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors)
{
	s32 retval;
	u8 cmd[10] = {SCSI_READ_CAPACITY, lun<<5};
	u8 response[8];

	retval = __cycle(dev, lun, response, sizeof(response), cmd, sizeof(cmd), 0, NULL, NULL);
	if(retval >= 0)
	{
		if(n_sectors != NULL)
			memcpy(n_sectors, response, 4);
		if(sector_size != NULL)
			memcpy(sector_size, response + 4, 4);
		retval = USBSTORAGE_OK;
	}

	return retval;
}

s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_READ_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >>  8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
	};

	if(lun >= dev->max_lun || dev->sector_size[lun] == 0)
		return IPC_EINVAL;

	retval = __cycle(dev, lun, buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 0, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;

	return retval;
}

s32 USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_WRITE_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >> 8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
	};

	if(lun >= dev->max_lun || dev->sector_size[lun] == 0)
		return IPC_EINVAL;
	retval = __cycle(dev, lun, (u8 *)buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 1, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
	return retval;
}

s32 USBStorage_Suspend(usbstorage_handle *dev)
{
	if(dev->suspended == 1)
		return USBSTORAGE_OK;

	USB_SuspendDevice(dev->usb_fd);
	dev->suspended = 1;

	return USBSTORAGE_OK;
}

/*
The following is for implementing a DISC_INTERFACE
as used by libfat
*/

static bool __usbstorage_Startup(void)
{
	if (__inited)
		return true;

	if (USB_Initialize() < 0 || USBStorage_Initialize() < 0)
		return false;

	return true;
}

static bool __usbstorage_IsInserted(void)
{
	usb_device_entry *buffer;
	u8 device_count;
	u8 i, j;
	u16 vid, pid;
	s32 maxLun;
	s32 retval;

	if (__mounted)
		return true;

	if (__removed) {
		__removed = false;
		return false;
	}

	if (!__inited)
		return false;

	buffer = (usb_device_entry*)__lwp_heap_allocate(&__heap, DEVLIST_MAXSIZE * sizeof(usb_device_entry));
	if (!buffer)
		return false;

	memset(buffer, 0, DEVLIST_MAXSIZE * sizeof(usb_device_entry));

	if (USB_GetDeviceList(buffer, DEVLIST_MAXSIZE, USB_CLASS_MASS_STORAGE, &device_count) < 0)
	{
		if (__vid != 0 || __pid != 0)
			USBStorage_Close(&__usbfd);

		__lwp_heap_free(&__heap, buffer);
		return false;
	}

	usleep(100);

	if (__vid != 0 || __pid != 0) {
		for(i = 0; i < device_count; i++) {
			vid = buffer[i].vid;
			pid = buffer[i].pid;
			if(vid != 0 || pid != 0) {
				if((vid == __vid) && (pid == __pid)) {
					__mounted = true;
					__lwp_heap_free(&__heap,buffer);

					// I don't know why I have to wait but it's needed
					usleep(50);

					return true;
				}
			}
		}
		USBStorage_Close(&__usbfd);
		__lwp_heap_free(&__heap,buffer);
		return false;
	}

	for (i = 0; i < device_count; i++) {
		vid = buffer[i].vid;
		pid = buffer[i].pid;
		if (vid == 0 || pid == 0)
			continue;

		if (USBStorage_Open(&__usbfd, buffer[i].device_id, vid, pid) < 0)
			continue;

		maxLun = USBStorage_GetMaxLUN(&__usbfd);
		for (j = 0; j < maxLun; j++) {
			retval = USBStorage_MountLUN(&__usbfd, j);

			if (retval == USBSTORAGE_ETIMEDOUT)
				break;

			if (retval < 0)
				continue;

			__mounted = true;
			__lun = j;
			__vid = vid;
			__pid = pid;

			break;
		}

		if (__mounted)
			break;

		USBStorage_Close(&__usbfd);
	}
	__lwp_heap_free(&__heap, buffer);
	return __mounted;
}

static bool __usbstorage_ReadSectors(u32 sector, u32 numSectors, void *buffer)
{
	s32 retval;

	if (!__mounted)
		return false;

	retval = USBStorage_Read(&__usbfd, __lun, sector, numSectors, buffer);

	return retval >= 0;
}

static bool __usbstorage_WriteSectors(u32 sector, u32 numSectors, const void *buffer)
{
	s32 retval;

	if (!__mounted)
		return false;

	retval = USBStorage_Write(&__usbfd, __lun, sector, numSectors, buffer);

	return retval >= 0;
}

static bool __usbstorage_ClearStatus(void)
{
	return true;
}

static bool __usbstorage_Shutdown(void)
{
	if (__vid != 0 || __pid != 0)
		USBStorage_Close(&__usbfd);

	return true;
}

DISC_INTERFACE __io_usbstorage = {
	DEVICE_TYPE_WII_USB,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_USB,
	(FN_MEDIUM_STARTUP)&__usbstorage_Startup,
	(FN_MEDIUM_ISINSERTED)&__usbstorage_IsInserted,
	(FN_MEDIUM_READSECTORS)&__usbstorage_ReadSectors,
	(FN_MEDIUM_WRITESECTORS)&__usbstorage_WriteSectors,
	(FN_MEDIUM_CLEARSTATUS)&__usbstorage_ClearStatus,
	(FN_MEDIUM_SHUTDOWN)&__usbstorage_Shutdown
};

#endif /* HW_RVL */
