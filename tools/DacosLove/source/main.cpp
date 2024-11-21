/*
priiloader/preloader 0.30 - A tool which allows to change the default boot up sequence on the Wii console

Copyright (C) 2008-2009  crediar

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
#include <string>


#include <gccore.h>
#include <wiiuse/wpad.h>
#include <sdcard/wiisd_io.h>
#include <fat.h>
#include <ogc/usb.h>
#include <ogc/machine/processor.h>
#include <ogc/machine/asm.h>

#include <sys/dir.h>
#include <malloc.h>
#include <vector>
#include <algorithm>
#include <time.h>

//Project files
#include "Global.h"
#include "elf.h"
#include "error.h"
#include "font.h"
#include "gecko.h"
#include "titles.h"
#include "state.h"
#include "dvd.h"
#include "IOS.h"
#include "HomebrewChannel.h"
#include "mem2_manager.h"
#include "rapidxml.hpp"
#include "input.h"

//loader files
#include "patches.h"
#include "loader.h"

//Bin include
#include "loader_bin.h"

#define VWII_MODE

extern "C"
{
	extern void __exception_closeall();
}

typedef struct {
	std::string app_name;
	std::string app_path;
	u8 HW_AHBPROT_ENABLED;
	std::vector<std::string> args;
}Binary_struct;

extern u8 error;
u8 _read8(u32 addr)
{
	//note : this is less "safe" then libogc's read8 but it sure works on mem2 (unlike libogc...)
	u8 x;
	asm volatile("lwz %0,0(%1) ; sync" : "=r"(x) : "b"(addr));
	//asm volatile("lwz %0,0(%1) ; sync" : "=r"(x) : "b"(0xc0000000 | addr));
	return x;
}
void _write8(u32 addr,u8 value)
{
	//libogc minus the force to use uncached mem2
	asm("stb %0,0(%1) ; eieio" : : "r"(value), "b"(addr));
	//changed to be able to use unchached mem2
	//asm("stb %0,0(%1) ; eieio" : "=r"(value) : "b"(0xc0000000 | addr));
	//weirdly the most stable ;>_>
	//*(vu8*)addr = value;

}
s32 __IOS_LoadStartupIOS()
{
	return 0;
}
u8 BootMainSysMenu( void )
{
	SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
	return 0;
}
void ClearScreen()
{
	printf("\x1b[2J");
	fflush(stdout);
	VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
	VIDEO_WaitVSync();
	printf("\n");
}

s8 BootDolFromMem(u8* binary, u8 HW_AHBPROT_ENABLED, struct __argv* args)
{
	if (binary == NULL)
		return -1;

	void* loader_addr = NULL;
	u8 ret = 1;

	try
	{
		//its an elf; lets start killing DVD :')
		if (DVDCheckCover())
		{
			gprintf("BootDolFromMem : excecuting StopDisc Async...");
			DVDStopDisc(true);
		}
		else
		{
			gprintf("BootDolFromMem : Skipping StopDisc -> no drive or disc in drive");
		}

		//prepare loader
		loader_addr = (void*)mem_align(32, loader_bin_size);
		if (!loader_addr)
			throw "failed to alloc the loader";

		memcpy(loader_addr, loader_bin, loader_bin_size);
		DCFlushRange(loader_addr, loader_bin_size);
		ICInvalidateRange(loader_addr, loader_bin_size);
		SET_LOADER_ADDRESS(loader_addr);


		gprintf("BootDolFromMem : shutting down...");

		ClearState();
		Input_Shutdown();

		if (DvdKilled() < 1)
		{
			gprintf("checking DVD drive...");
			while (DvdKilled() < 1);
		}

		s8 bAHBPROT = 0;
		s32 Ios_to_load = 0;
		if (read32(0x0d800064) == 0xFFFFFFFF)
			bAHBPROT = HW_AHBPROT_ENABLED;
		if (!isIOSstub(58))
		{
			Ios_to_load = 58;
		}
		else if (!isIOSstub(61))
		{
			Ios_to_load = 61;
		}
		else if (!isIOSstub(IOS_GetPreferredVersion()))
		{
			Ios_to_load = IOS_GetPreferredVersion();
		}
		else
		{
			PrintFormat(1, TEXT_OFFSET("failed to reload ios for homebrew! ios is a stub!"), 208, "failed to reload ios for homebrew! ios is a stub!");
			sleep(2);
		}

		if (Ios_to_load > 2 && Ios_to_load < 255)
		{
			ReloadIos(Ios_to_load, &bAHBPROT);
			system_state.ReloadedIOS = 1;
		}

		gprintf("IOS state : ios %d - ahbprot : %d ", IOS_GetVersion(), (read32(0x0d800064) == 0xFFFFFFFF));

		ISFS_Deinitialize();
		ShutdownDevices();
		USB_Deinitialize();
		__IOS_ShutdownSubsystems();
		if (system_state.Init)
		{
			VIDEO_Flush();
			VIDEO_WaitVSync();
		}
		__exception_closeall();

		gprintf("BootDolFromMem : starting binary... 0x%08X", loader_addr);
		ICSync();
		loader(binary, args, args != NULL, 0);

		//old alternate booting code. i prefer the loader xD
		/*u32 level;
		__STM_Close();
		ISFS_Deinitialize();
		ShutdownDevices();
		USB_Deinitialize();
		__IOS_ShutdownSubsystems();
		SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
		_CPU_ISR_Disable (level);
		mtmsr(mfmsr() & ~0x8000);
		mtmsr(mfmsr() | 0x2002);
		ICSync();
		entrypoint();
		_CPU_ISR_Restore (level);*/

		//it failed. FAIL!
		gprintf("this ain't good");
		__IOS_InitializeSubsystems();
		PollDevices();
		ISFS_Initialize();
		Input_Init();
		PAD_Init();
		ret = -1;

	}
	catch (const std::string & ex)
	{
		gprintf("BootDolFromMem Exception -> %s", ex.c_str());
		ret = -7;
	}
	catch (char const* ex)
	{
		gprintf("BootDolFromMem Exception -> %s", ex);
		ret = -7;
	}
	catch (...)
	{
		gprintf("BootDolFromMem Exception was thrown");
		ret = -7;
	}

	if (loader_addr)
		mem_free(loader_addr);

	return ret;
}

s8 BootDolFromFile(const char* Dir, u8 HW_AHBPROT_ENABLED, const std::vector<std::string>& args_list)
{
	struct __argv* args = NULL;
	u8* binary = NULL;
	s8 ret = 1;
	FILE* dol = NULL;

	try
	{
		if (GetMountedValue() == 0)
		{
			return -5;
		}

		std::string _path;
		_path.append(Dir);
		gprintf("going to boot %s", _path.c_str());

		args = (struct __argv*)mem_align(32, sizeof(__argv));
		if (!args)
			throw "arg malloc failed";

		memset(args, 0, sizeof(__argv));

		args->argvMagic = 0;
		args->length = 0;
		args->commandLine = NULL;
		args->argc = 0;
		args->argv = &args->commandLine;
		args->endARGV = args->argv + 1;

		//calculate the char lenght of the arguments
		args->length = _path.size() + 1;
		//loading args
		for (u32 i = 0; i < args_list.size(); i++)
		{
			if (args_list[i].c_str())
				args->length += strnlen(args_list[i].c_str(), 128) + 1;
		}

		//allocate memory for the arguments
		args->commandLine = (char*)mem_malloc(args->length);
		if (args->commandLine == NULL)
		{
			args->commandLine = 0;
			args->length = 0;
		}
		else
		{
			//assign arguments and the rest
			strcpy(&args->commandLine[0], _path.c_str());

			//add the other arguments
			args->argc = 1;

			if (args_list.size() > 0)
			{
				u32 pos = _path.size() + 1;
				for (u32 i = 0; i < args_list.size(); i++)
				{
					if (args_list[i].c_str())
					{
						strcpy(&args->commandLine[pos], args_list[i].c_str());
						pos += strlen(args_list[i].c_str()) + 1;
					}
				}
				args->argc += args_list.size();
			}

			args->commandLine[args->length - 1] = '\0';
			args->argv = &args->commandLine;
			args->endARGV = args->argv + 1;
			args->argvMagic = ARGV_MAGIC; //everything is set so the magic is set so it becomes valid*/
		}
		dol = fopen(Dir, "rb");
		if (!dol)
			throw "failed to open file";

		fseek(dol, 0, SEEK_END);
		u32 size = ftell(dol);
		fseek(dol, 0, 0);

		binary = (u8*)mem_align(32, ALIGN32(size));
		if (!binary)
			throw "failed to alloc the binary";

		memset(binary, 0, size);
		fread(binary, sizeof(u8), size, dol);
		fclose(dol);

		BootDolFromMem(binary, HW_AHBPROT_ENABLED, args);
		//we didn't boot, that ain't good
		ret = -8;
	}
	catch (const std::string & ex)
	{
		gprintf("BootDolFromFile Exception -> %s", ex.c_str());
		ret = -7;
	}
	catch (char const* ex)
	{
		gprintf("BootDolFromFile Exception -> %s", ex);
		ret = -7;
	}
	catch (...)
	{
		gprintf("BootDolFromFile Exception was thrown");
		ret = -7;
	}

	if (dol)
		fclose(dol);
	if (binary)
		mem_free(binary);
	if (args != NULL && args->commandLine != NULL)
		mem_free(args->commandLine);
	if (args)
		mem_free(args);

	return ret;
}

void InstallLoadDOL(void)
{
	char filename[NAME_MAX + 1], filepath[MAXPATHLEN + NAME_MAX + 1];
	memset(filename, 0, NAME_MAX + 1);
	memset(filepath, 0, MAXPATHLEN + NAME_MAX + 1);
	std::vector<Binary_struct> app_list;
	DIR* dir;
	s8 reload = 1;
	s8 redraw = 1;
	s8 DevStat = GetMountedValue();
	s16 cur_off = 0;
	s16 max_pos = 0;
	s16 min_pos = 0;
	u32 ret = 0;
	while (1)
	{
		PollDevices();
		if (DevStat != GetMountedValue())
		{
			ClearScreen();
			PrintFormat(1, TEXT_OFFSET("Reloading Binaries..."), 208, "Reloading Binaries...");
			sleep(1);
			app_list.clear();
			reload = 1;
			min_pos = 0;
			max_pos = 0;
			cur_off = 0;
			redraw = 1;
		}
		if (GetMountedValue() == 0)
		{
			ClearScreen();
			PrintFormat(1, TEXT_OFFSET("NO fat device found!"), 208, "NO fat device found!");
			sleep(5);
			return;
		}
		if (reload)
		{
			gprintf("loading binaries...");
			DevStat = GetMountedValue();
			reload = 0;
			dir = opendir("fat:/apps/");
			if (dir != NULL)
			{
				//get all files names
				while (readdir(dir) != NULL)
				{
					memset(filename, 0, NAME_MAX);
					memcpy(filename, dir->fileData.d_name, strnlen(dir->fileData.d_name, NAME_MAX));
					if (strncmp(filename, ".", 1) == 0 || strncmp(filename, "..", 2) == 0)
					{
						//we dont want the root or the dirup stuff. so lets filter them
						continue;
					}
					sprintf(filepath, "fat:/apps/%s/boot.dol", filename);
					FILE* app_bin;
					app_bin = fopen(filepath, "rb");
					if (!app_bin)
					{
						sprintf(filepath, "fat:/apps/%s/boot.elf", filename);
						app_bin = fopen(filepath, "rb");
						if (!app_bin)
							continue;
					}
					fclose(app_bin);
					Binary_struct temp;
					temp.HW_AHBPROT_ENABLED = 0;
					temp.app_path = filepath;
					temp.args.clear();
					sprintf(filepath, "fat:/apps/%s/meta.xml", filename);
					app_bin = fopen(filepath, "rb");
					if (!app_bin)
					{
						gdprintf("failed to open meta.xml of %s", filename);
						temp.app_name = filename;
						app_list.push_back(temp);
						continue;
					}
					long size;
					char* buf;

					fseek(app_bin, 0, SEEK_END);
					size = ftell(app_bin);
					rewind(app_bin);
					buf = (char*)mem_malloc(size + 1);
					if (!buf)
					{
						gdprintf("buf == NULL");
						fclose(app_bin);
						temp.app_name = filename;
						app_list.push_back(temp);
						continue;
					}
					memset(buf, 0, size + 1);
					ret = fread(buf, 1, size, app_bin);
					fclose(app_bin);
					if (ret != (u32)size)
					{
						mem_free(buf);
						temp.app_name = filename;
						app_list.push_back(temp);
						gdprintf("failed to read data error %d", ret);
						continue;
					}

					try
					{
						rapidxml::xml_document<> doc;
						doc.parse<0>(buf);
						rapidxml::xml_node<>* appNode = doc.first_node("app");
						rapidxml::xml_node<>* node = NULL;
						if (appNode == NULL)
							throw "No app node found";

						node = appNode->first_node("name");
						if (node != NULL)
							temp.app_name = (std::string)node->value();

						if (appNode->first_node("no_ios_reload") != NULL || appNode->first_node("ahb_access") != NULL)
							temp.HW_AHBPROT_ENABLED = 1;

						node = appNode->first_node("arguments");
						if (node != NULL)
						{
							rapidxml::xml_node<>* arg = node->first_node("arg");
							while (arg != NULL)
							{
								temp.args.push_back((std::string)arg->value());
								arg = arg->next_sibling("arg");
							}
						}
					}
					catch (const std::string & str)
					{
						gprintf("InstallLoadDol Exception: %s", str.c_str());
					}
					catch (char const* str)
					{
						gprintf("InstallLoadDol Exception: %s", str);
					}
					catch (...)
					{
						gprintf("InstallLoadDol Exception: General xml exception");
					}

					mem_free(buf);
					if (temp.app_name.size())
					{
						gdprintf("added %s to list", temp.app_name.c_str());
						app_list.push_back(temp);
						continue;
					}
					else
					{
						temp.app_name = filename;
						app_list.push_back(temp);
						gdprintf("no name found in xml D:<");
						continue;
					}
				}
				closedir(dir);
			}
			else
			{
				gprintf("WARNING: could not open fat:/apps/ for binaries");
			}
			dir = opendir("fat:/");
			if (dir == NULL)
			{
				gprintf("WARNING: could not open fat:/ for binaries");
			}
			else
			{
				while (readdir(dir) != NULL)
				{
					memset(filename, 0, NAME_MAX);
					memcpy(filename, dir->fileData.d_name, strnlen(dir->fileData.d_name, NAME_MAX));
					if ((strstr(filename, ".dol") != NULL) ||
						(strstr(filename, ".DOL") != NULL) ||
						(strstr(filename, ".elf") != NULL) ||
						(strstr(filename, ".ELF") != NULL))
					{
						Binary_struct temp;
						temp.HW_AHBPROT_ENABLED = 0;
						temp.app_name = filename;
						sprintf(filepath, "fat:/%s", filename);
						temp.app_path.assign(filepath);
						app_list.push_back(temp);
					}
				}
				closedir(dir);
			}

			if (app_list.size() == 0)
			{
				if ((GetMountedValue() & 2) && ToggleUSBOnlyMode() == 1)
				{
					gprintf("switching to USB only...also mounted == %d", GetMountedValue());
					reload = 1;
					continue;
				}
				if (GetUsbOnlyMode() == 1)
				{
					gprintf("fixing usbonly mode...");
					ToggleUSBOnlyMode();
				}
				PrintFormat(1, TEXT_OFFSET("Couldn't find any executable files"), 208, "Couldn't find any executable files");
				PrintFormat(1, TEXT_OFFSET("in the fat:/apps/ on the device!"), 228, "in the fat:/apps/ on the device!");
				sleep(5);
				return;
			}
			if (VI_TVMODE_ISFMT(rmode->viTVMode, VI_NTSC) || CONF_GetEuRGB60() || CONF_GetProgressiveScan())
			{
				//ye, those tv's want a special treatment again >_>
				max_pos = 14;
			}
			else
			{
				max_pos = 19;
			}
			if ((s32)app_list.size() <= max_pos)
				max_pos = app_list.size() - 1;

			//sort app lists
			s8 swap = 0;
			for (s32 max = 0; max < (s32)app_list.size() * (s32)app_list.size(); max++)
			{
				swap = 0;
				for (int count = 0; count < (s32)app_list.size(); count++)
				{
					if (strncmp(app_list[count].app_path.c_str(), "fat:/apps/", strlen("fat:/apps/")) == 0)
					{
						for (int swap_index = 0; swap_index < (s32)app_list.size(); swap_index++)
						{
							if (strcasecmp(app_list[count].app_name.c_str(), app_list[swap_index].app_name.c_str()) < 0)
							{
								swap = 1;
								std::swap(app_list[count], app_list[swap_index]);
							}
						}
					}
				}
				if (swap != 0)
					break;
			}
			ClearScreen();
			redraw = true;
		}
		if (redraw)
		{
			s16 i = min_pos;
			if ((s32)app_list.size() - 1 > max_pos && (min_pos != (s32)app_list.size() - max_pos - 1))
			{
				PrintFormat(0, TEXT_OFFSET("-----More-----"), 64 + (max_pos + 2) * 16, "-----More-----");
			}
			if (min_pos > 0)
			{
				PrintFormat(0, TEXT_OFFSET("-----Less-----"), 64, "-----Less-----");
			}
			for (; i <= (min_pos + max_pos); i++)
			{
				PrintFormat(cur_off == i, 16, 64 + (i - min_pos + 1) * 16, "%s%s", app_list[i].app_name.c_str(), (read32(0x0d800064) == 0xFFFFFFFF && app_list[i].HW_AHBPROT_ENABLED != 0) ? "(AHBPROT Available)" : " ");
			}
			PrintFormat(0, TEXT_OFFSET("1(A) Load File   "), rmode->viHeight - 96, "1(A) Load File");
			redraw = false;
		}

		Input_ScanPads();
		u32 pressed = Input_ButtonsDown();

		if (pressed & INPUT_BUTTON_B)
		{
			break;
		}
		if (pressed & INPUT_BUTTON_A)
		{
			ClearScreen();
			PrintFormat(1, TEXT_OFFSET("Loading binary..."), 208, "Loading binary...");
			ret = BootDolFromFile(app_list[cur_off].app_path.c_str(), app_list[cur_off].HW_AHBPROT_ENABLED, app_list[cur_off].args);
			gprintf("loading %s ret %d", app_list[cur_off].app_path.c_str(), ret);
			PrintFormat(1, TEXT_OFFSET("failed to load binary"), 224, "failed to load binary");
			sleep(3);
			ClearScreen();
			redraw = true;
		}
		if (pressed & INPUT_BUTTON_UP)
		{
			cur_off--;
			if (cur_off < min_pos)
			{
				min_pos = cur_off;
				if (app_list.size() > 19)
				{
					for (s8 i = min_pos; i <= (min_pos + max_pos); i++)
					{
						PrintFormat(0, 16, 64 + (i - min_pos + 1) * 16, "                                                            ");
						PrintFormat(0, TEXT_OFFSET("               "), 64 + (max_pos + 2) * 16, "               ");
						PrintFormat(0, TEXT_OFFSET("               "), 64, "               ");
					}
				}
			}
			if (cur_off < 0)
			{
				cur_off = app_list.size() - 1;
				min_pos = app_list.size() - max_pos - 1;
			}
			redraw = true;
		}
		if (pressed & INPUT_BUTTON_DOWN)
		{
			cur_off++;
			if (cur_off > (max_pos + min_pos))
			{
				min_pos = cur_off - max_pos;
				if (app_list.size() > 19)
				{
					for (s8 i = min_pos; i <= (min_pos + max_pos); i++)
					{
						PrintFormat(0, 16, 64 + (i - min_pos + 1) * 16, "                                                            ");
						PrintFormat(0, TEXT_OFFSET("               "), 64 + (max_pos + 2) * 16, "               ");
						PrintFormat(0, TEXT_OFFSET("               "), 64, "               ");
					}
				}
			}
			if (cur_off >= (s32)app_list.size())
			{
				cur_off = 0;
				min_pos = 0;
			}
			redraw = true;
		}
		VIDEO_WaitVSync();
	}

	//free memory
	app_list.clear();
	return;
}

u32 GetSysMenuVersion(void)
{
	//Get sysversion from TMD
	u64 TitleID = 0x0000000100000002LL;
	u32 tmd_size;
	s32 r = ES_GetTMDViewSize(TitleID, &tmd_size);
	if (r < 0)
	{
		gprintf("SysMenuVersion : GetTMDViewSize error %d", r);
		return 0;
	}

	tmd_view* rTMD = (tmd_view*)mem_align(32, ALIGN32(tmd_size));
	if (rTMD == NULL)
	{
		gdprintf("SysMenuVersion : memalign failure");
		return 0;
	}
	memset(rTMD, 0, tmd_size);
	r = ES_GetTMDView(TitleID, (u8*)rTMD, tmd_size);
	if (r < 0)
	{
		gprintf("SysMenuVersion : GetTMDView error %d", r);
		mem_free(rTMD);
		return 0;
	}

	u32 sysver = rTMD->title_version;
	mem_free(rTMD);
	return sysver;
}

u8 PatchIOS( void )
{
	if(read32(0x0d800064) != 0xFFFFFFFF)
	{
		printf("AHBPROT Not Enabled!\n");
		sleep(2);
		return 1;
	}

	printf("AHBPROT enabled :') \n");
	if(read16(0x0d8b420a))
	{
		printf("setting memory access...\n");
		write16(0x0d8b420a, 0);
		write16(0x0C00401C,0); 
		write16(0x0C004010,0xFFFF);
		write32(0x0C004000,0);
		write32(0x0C004004,0);
		write32(0x0C004008,0);
		write32(0x0C00400C,0);
		write16(0x0d8b420a, 0);
		write16(0x0d8b420c, (((u32)NULL) & 0xFFFFFFF) >> 12);
		write16(0x0d8b420e, (((u32)NULL) & 0xFFFFFFF) >> 12);
	}

	if (read16(0x0d8b420a))
	{
		printf("failed to set memory access\n");
		sleep(2);
		return 1;
	}

	printf("start looking for the function(s)...\n");

	//setuid : 0xD1, 0x2A, 0x1C, 0x39 -> 0x46, 0xC0, 0x1C, 0x39
	static const u8 setuid_old[] = { 0xD1, 0x2A, 0x1C, 0x39 };

	//ahbprot on reload : 68 1B -> 0x23 0xFF
	static const u16 es_set_ahbprot[] = {
	  0x685B,          // ldr r3,[r3,#4]  ; get TMD pointer
	  0x22EC, 0x0052,  // movls r2, 0x1D8
	  0x189B,          // adds r3, r3, r2 ; add offset of access rights field in TMD
	  0x681B,          // ldr r3, [r3]    ; load access rights (haxxme!)
	  0x4698,          // mov r8, r3      ; store it for the DVD video bitcheck later
	  0x07DB           // lsls r3, r3, #31; check AHBPROT bit
	}; //patch by tuedj

	//hash of dip module
	static const u8 Cios_Dip[] = { 0x4B, 0x00, 0x47, 0x18 };

	//nand permissions : 42 8b d0 01 25 66 -> 42 8b e0 01 25 66
	static const u8 old_nand_Table[] = { 0x42, 0x8B, 0xD0, 0x01, 0x25, 0x66 };

	//allow downgrading titles, thanks daveboal : D2 -> 0xE0
	static const u8 downgrade_fix[] = { 0x20, 0xEE, 0x00, 0x40, 0x18, 0x2B, 0x1C, 0x3A, 0x32, 0x58, 0x88, 0x19, 0x88, 0x13, 0x42, 0x99, 0xD2 };

	//allow fake signature titles to be installed : 07 -> 00
	static const u8 trucha_hash1[] = { 0x20, 0x07, 0x23, 0xA2 };
	static const u8 trucha_hash2[] = { 0x20, 0x07, 0x4B, 0x0B };

	//allow identification of 'anything' : D1 23 -> 00 00
	static const u8 identifyCheck[] = { 0x68, 0x68, 0x28, 0x03, 0xD1, 0x23 };

	//------------------------------
	//vWii specific patches :
	//------------------------------
	//these patches should allow us to install 00000001 titles again (aka system titles like IOS and SM)
#ifdef VWII_MODE
	u8 SysTitleInstall_pt1[] = { 0x68, 0x1A, 0x2A, 0x01, 0xD0, 0x05 }; //needs to be applied twice
	u8 SysTitleInstall_pt2[] = { 0xD0, 0x02, 0x33, 0x06, 0x42, 0x9A, 0xD1, 0x01 };	//also twice
	u8 SysTitleInstall_pt3[] = { 0x68, 0xFB, 0x2B, 0x00, 0xDB, 0x01 }; //once
	u8 pt1_found = 0;
	u8 pt2_found = 0;
	u8 pt3_found = 0;
#endif

	//patching. according to sven writex is better then vu pointers (but failed before obviously) and even better would be to load ios(into memory) from nand,patch it, and let starlet load it
	u8* mem_block = (u8*)read32(0x80003130);
	s8 patches_applied = 0;
	while((u32)mem_block < 0x93AFFFFF)
	{
		u32 address = (u32)mem_block;
		if (!memcmp(mem_block, Cios_Dip, sizeof(Cios_Dip)))
		{
			//NOTE : 0x202003B8 (address in Cios DIP source) == 0x939B03B8 ( so cios dip address + 0x737B0000 = mem2 cached address )
			u32 added_addr = 0;
			switch ((u16)address)
			{
			case 0x03B8:
				added_addr = 0x974;
				break;
			case 0x0400:
				added_addr = 0xAF8;
				break;
			default:
				break;

			}
			if (added_addr != 0)
			{
				if (!memcmp(mem_block + added_addr, Cios_Dip, sizeof(Cios_Dip)))
				{
					printf("Found CIOS Dip @ 0x%X\r\n", address);
					printf("F\r\nA\r\nI\r\nL\r\n");
				}
			}
		}

		if (!memcmp(mem_block, setuid_old, sizeof(setuid_old)))
		{
			printf("Found SetUID @ 0x%X, patching...\r\n", address);
			_write8(address, 0x46);
			_write8(address + 1, 0xC0);
			patches_applied++;
			DCFlushRange((u8*)((address) >> 5 << 5), (sizeof(setuid_old) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X & 0x%X\r\n", *(vu32*)(address), *(vu32*)(address + 4));
#endif
			goto continue_loop;
		}

		if (!memcmp(mem_block, old_nand_Table, sizeof(old_nand_Table)))
		{
			printf("Found NAND Permission check @ 0x%X, patching...\r\n", address);
			_write8(address + 2, 0xE0);
			_write8(address + 3, 0x01);
			patches_applied++;
			DCFlushRange((u32*)address, 64);
			ICInvalidateRange((u8*)((address) >> 5 << 5), (sizeof(old_nand_Table) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X\r\n", *(vu32*)address);
#endif
			goto continue_loop;
		}
		if (!memcmp(mem_block, es_set_ahbprot, sizeof(es_set_ahbprot)))
		{
			printf("Found ES_AHBPROT check @ 0x%X, patching...\r\n", address);
			_write8(address + 8, 0x23); // li r3, 0xFF.aka, make it look like the TMD had max settings
			_write8(address + 9, 0xFF);
			patches_applied++;
			DCFlushRange((u8*)((address) >> 5 << 5), (sizeof(es_set_ahbprot) >> 5 << 5) + 64);
			ICInvalidateRange((u8*)((address) >> 5 << 5), (sizeof(es_set_ahbprot) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X\r\n", *(vu32*)address);
#endif
			goto continue_loop;
		}
		if (!memcmp(mem_block, trucha_hash1, sizeof(trucha_hash1)))
		{
			printf("Found Hash check @ 0x%X, patching...\r\n", address);
			_write8(address + 1, 0);
			patches_applied++;
			DCFlushRange((u8*)((address) >> 5 << 5), (sizeof(trucha_hash1) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X\r\n", *(vu32*)address);
#endif
			goto continue_loop;
		}
		if (!memcmp(mem_block, trucha_hash2, sizeof(trucha_hash2)))
		{
			printf("Found Hash check2 @ 0x%X, patching...\r\n", address);
			_write8(address + 1, 0);
			patches_applied++;
			DCFlushRange((u8*)((address) >> 5 << 5), (sizeof(trucha_hash2) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X\r\n", *(vu32*)address);
#endif
			goto continue_loop;
		}
		if (!memcmp(mem_block, identifyCheck, sizeof(identifyCheck)))
		{
			printf("Found ES_Identify check @ 0x%X, patching...\r\n", address);
			_write8(address + 4, 0x00);
			_write8(address + 5, 0x00);
			patches_applied++;
			DCFlushRange((u8*)((address) >> 5 << 5), (sizeof(identifyCheck) >> 5 << 5) + 64);
			ICInvalidateRange((u8*)((address) >> 5 << 5), (sizeof(identifyCheck) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X & 0x%X\r\n", *(vu32*)(address), *(vu32*)(address + 4));
#endif
			goto continue_loop;
		}
		if (!memcmp(mem_block, downgrade_fix, sizeof(downgrade_fix)))
		{
			printf("Found downgrade check @ 0x%X, patching...\r\n", address);
			_write8(address + 16, 0xE0);
			patches_applied++;
			DCFlushRange((u8*)((address) >> 5 << 5), (sizeof(downgrade_fix) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X\r\n", *(vu32*)(address + 13));
#endif
			goto continue_loop;
		}

#ifdef VWII_MODE
		if (!memcmp(mem_block, SysTitleInstall_pt1, sizeof(SysTitleInstall_pt1)))
		{
			printf("Found pt1 of SysInstall check @ 0x%X, patching...\r\n", address);
			_write8(address + 4, 0x46);
			_write8(address + 5, 0xC0);
			pt1_found++;
			if (pt1_found >= 2)
				patches_applied++;
			DCFlushRange((u8*)((address) >> 5 << 5), (sizeof(SysTitleInstall_pt1) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X\r\n", *(vu32*)(address + 4));
#endif
			goto continue_loop;
		}
		if (!memcmp(mem_block, SysTitleInstall_pt2, sizeof(SysTitleInstall_pt2)))
		{
			printf("Found pt2 of SysInstall check @ 0x%X, patching...\r\n", address);
			_write8(address, 0x46);
			_write8(address + 1, 0xC0);
			_write8(address + 6, 0xE0);
			pt2_found++;
			if (pt2_found >= 2)
				patches_applied++;
			DCFlushRange((u8*)((address) >> 5 << 5), (sizeof(SysTitleInstall_pt1) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X\r\n", *(vu32*)(address + 4));
#endif
			goto continue_loop;
		}
		if (!memcmp(mem_block, SysTitleInstall_pt3, sizeof(SysTitleInstall_pt3)))
		{
			printf("Found pt3 of SysInstall check @ 0x%X, patching...\r\n", address);
			_write8(address + 5, 0x10);
			pt3_found++;
			patches_applied++;
			DCFlushRange((u8*)((address) >> 5 << 5), (sizeof(SysTitleInstall_pt1) >> 5 << 5) + 64);
#ifdef DEBUG
			printf("value is now 0x%X\r\n", *(vu32*)(address + 4));
#endif
			goto continue_loop;
		}
#endif

continue_loop:
		mem_block++;
		continue;
	}

#ifdef VWII_MODE
	if (pt1_found != pt2_found || pt2_found != pt3_found)
	{
		//the SysTitle patch is incomplete. for safty , lets abort
		printf("SysTitleInstall not complete. please report this!\r\n");
		sleep(5);
	}
#endif

	printf("%d IOS Patches applied\n",patches_applied);
	sleep(2);
	return 1;
}

void HandleWiiMoteEvent(s32 chan)
{
	if (system_state.InMainMenu == 0)
		return;
	system_state.Shutdown = 1;
	return;
}

int main(int argc, char **argv)
{
	CheckForGecko();
	gprintf("Daco's Love : Priiloader core rev 148 & 0.8B1");
	gprintf("Built   : %s %s", __DATE__, __TIME__ );
	gprintf("Version : 0.2");
	gprintf("Firmware: %d.%d.%d", *(vu16*)0x80003140, *(vu8*)0x80003142, *(vu8*)0x80003143 );

	*(vu32*)0x80000020 = 0x0D15EA5E;				// Magic word (how did the console boot?)
	*(vu32*)0x800000F8 = 0x0E7BE2C0;				// Bus Clock Speed
	*(vu32*)0x800000FC = 0x2B73A840;				// CPU Clock Speed

	*(vu32*)0x80000040 = 0x00000000;				// Debugger present?
	*(vu32*)0x80000044 = 0x00000000;				// Debugger Exception mask
	*(vu32*)0x80000048 = 0x00000000;				// Exception hook destination 
	*(vu32*)0x8000004C = 0x00000000;				// Temp for LR
	*(vu32*)0x80003100 = 0x01800000;				// Physical Mem1 Size
	*(vu32*)0x80003104 = 0x01800000;				// Console Simulated Mem1 Size

	*(vu32*)0x80003118 = 0x04000000;				// Physical Mem2 Size
	*(vu32*)0x8000311C = 0x04000000;				// Console Simulated Mem2 Size

	*(vu32*)0x80003120 = 0x93400000;				// MEM2 end address ?

	s32 r = ISFS_Initialize();
	if( r < 0 )
	{
		*(vu32*)0xCD8000C0 |= 0x20;
		error=ERROR_ISFS_INIT;
	}

	AddMem2Area(14 * 1024 * 1024, OTHER_AREA);
	LoadHBCStub();
	AUDIO_Init (NULL);
	DSP_Init ();
	AUDIO_StopDMA();
	AUDIO_RegisterDMACallback(NULL);

	r = (s32)PollDevices();
	gprintf("FAT_Init():%d", r );

	r = PAD_Init();
	gprintf("PAD_Init():%d", r );

	r = WPAD_Init();
	gprintf("WPAD_Init():%d", r );

	WPAD_SetPowerButtonCallback(HandleWiiMoteEvent);
	Input_Init();
	InitVideo();
	ClearScreen();

	s32 cur_off=0;
	u32 redraw=true;
	u32 SysVersion=GetSysMenuVersion();
	while(1)
	{
		WPAD_ScanPads();
		PAD_ScanPads();

		u32 WPAD_Pressed = WPAD_ButtonsDown(0) | WPAD_ButtonsDown(1) | WPAD_ButtonsDown(2) | WPAD_ButtonsDown(3);
		u32 PAD_Pressed  = PAD_ButtonsDown(0) | PAD_ButtonsDown(1) | PAD_ButtonsDown(2) | PAD_ButtonsDown(3);
		if ( WPAD_Pressed & WPAD_BUTTON_A || WPAD_Pressed & WPAD_CLASSIC_BUTTON_A || PAD_Pressed & PAD_BUTTON_A )
		{
			system_state.InMainMenu = 0;
			ClearScreen();
			switch(cur_off)
			{
				case 0:		//Load HBC
					LoadHBC();
					break;
				case 1: //Load Bootmii
					LoadBootMii();
					//well that failed...
					error=ERROR_BOOT_BOOTMII;
					break;
				case 2: // show titles list
					LoadListTitles();
					break;
				case 3:
					InstallLoadDOL();
					break;
				case 4:
					PatchIOS();
					break;
				case 5: //exit to SM
					BootMainSysMenu();
					if(!error)
						error=ERROR_SYSMENU_GENERAL;
					break;
				default:
					break;

			}
			system_state.InMainMenu = 1;
			ClearScreen();
			redraw=true;
		}

		if ( WPAD_Pressed & WPAD_BUTTON_DOWN || WPAD_Pressed & WPAD_CLASSIC_BUTTON_DOWN || PAD_Pressed & PAD_BUTTON_DOWN )
		{
			cur_off++;
			if( cur_off >= 6 )
				cur_off = 0;
			redraw=true;
		} else if ( WPAD_Pressed & WPAD_BUTTON_UP || WPAD_Pressed & WPAD_CLASSIC_BUTTON_UP || PAD_Pressed & PAD_BUTTON_UP )
		{
			cur_off--;

			if( cur_off < 0 )
			{
				cur_off=5;
			}
			redraw=true;
		}

		if( redraw )
		{
			PrintFormat( 0, (rmode->viWidth - (strlen("Daco's Love v0.2")*13/2)), rmode->viHeight-48, "Daco's Love v0.2");
			PrintFormat( 0, 16, rmode->viHeight-64, "IOS v%d", (*(vu32*)0x80003140)>>16 );
			PrintFormat( 0, 16, rmode->viHeight-48, "Systemmenu v%d", SysVersion );			
			PrintFormat( 0, 16, rmode->viHeight-20, "Daco's Love = Priiloader");
			PrintFormat( cur_off==0, ((rmode->viWidth /2)-((strlen("Homebrew Channel"))*13/2))>>1, 80, "Homebrew Channel");
			PrintFormat( cur_off==1, ((rmode->viWidth /2)-((strlen("BootMii IOS"))*13/2))>>1, 96, "BootMii IOS");
			PrintFormat( cur_off==2, ((rmode->viWidth /2)-((strlen("Launch Title"))*13/2))>>1, 112, "Launch Title");
			PrintFormat( cur_off==3, ((rmode->viWidth /2)-((strlen("Load Binary"))*13/2))>>1, 128, "Load Binary");
			PrintFormat( cur_off==4, ((rmode->viWidth /2)-((strlen("Patch IOS"))*13/2))>>1, 144, "Patch IOS");
			PrintFormat( cur_off==5, ((rmode->viWidth /2)-((strlen("Exit To System Menu"))*13/2))>>1, 160, "Exit To System Menu");
			if (error > 0)
			{
				ShowError();
				error = ERROR_NONE;
			}
			redraw = false;
		}
		if(system_state.Shutdown )
		{
			*(vu32*)0xCD8000C0 &= ~0x20;
			//when we are in preloader itself we should make the video black or de-init it before the user thinks its not shutting down...
			//TODO : fade to black if possible without a gfx lib?
			//STM_SetVIForceDimming ?
			ClearState();
			VIDEO_ClearFrameBuffer( rmode, xfb, COLOR_BLACK);
			DVDStopDisc(false);
			WPAD_Shutdown();
			ShutdownDevices();
			USB_Deinitialize();
			if( CONF_GetShutdownMode() == CONF_SHUTDOWN_STANDBY )
			{
				//standby = red = off
				STM_ShutdownToStandby();
			}
			else
			{
				//idle = orange = standby
				s32 ret;
				ret = CONF_GetIdleLedMode();
				if (ret >= 0 && ret <= 2)
					STM_SetLedMode(ret);
				STM_ShutdownToIdle();
			}
		}

		//check Mounted Devices
		PollDevices();
		
		VIDEO_WaitVSync();
	}

	return 0;
}
