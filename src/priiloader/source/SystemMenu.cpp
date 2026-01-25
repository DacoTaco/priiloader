/*
SystemMenu.cpp - SystemMenu utils for priiloader

Copyright (C) 2022
GaryOderNichts
DacoTaco

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
#include <malloc.h>
#include <string.h>
#include <ogc/sha.h>

#include "gecko.h"
#include "error.h"
#include "SystemMenu.h"
#include "mem2_manager.h"
#include "executables.h"
#include "loader.h"
#include "hacks.h"
#include "titles.hpp"
#include "Input.h"
#include "settings.h"
#include "IOS.hpp"
#include "state.h"
#include "patches.h"
#include "Video.h"
#include "vWii.h"

//files
#include "loader_bin.h"

extern "C"
{
	extern void __exception_closeall();
}

extern void _mountCallback(bool sd_mounted, bool usb_mounted);

static s32 sysver = -1;
u32 GetSysMenuIOS( void )
{
	//Get sysversion from TMD
	u64 TitleID = SYSTEMMENU_TITLEID;
	u32 tmd_size;

	s32 r = ES_GetTMDViewSize(TitleID, &tmd_size);
	if(r<0)
	{
		gprintf("GetSysMenuIOS : GetTMDViewSize error %d",r);
		return 0;
	}

	tmd_view *rTMD = static_cast<tmd_view*>(mem_align( 32, ALIGN32(tmd_size) ));
	if( rTMD == NULL )
	{
		return 0;
	}
	memset(rTMD,0, tmd_size );
	r = ES_GetTMDView(TitleID, rTMD, tmd_size);
	if(r<0)
	{
		gprintf("GetSysMenuIOS : GetTMDView error %d",r);
		mem_free( rTMD );
		return 0;
	}
	u8 IOS = rTMD->title_version;

	mem_free(rTMD);
	return IOS;
}

u32 GetSysMenuVersion( void )
{
	if(sysver >= 0)
		return sysver;

	//Get sysversion from TMD
	u64 TitleID = SYSTEMMENU_TITLEID;
	u32 tmd_size;
	s32 r = ES_GetTMDViewSize(TitleID, &tmd_size);
	if(r<0)
	{
		gprintf("SysMenuVersion : GetTMDViewSize error %d",r);
		return 0;
	}

	tmd_view *rTMD = static_cast<tmd_view*>(mem_align( 32, ALIGN32(tmd_size) ));
	if( rTMD == NULL )
	{
		gdprintf("SysMenuVersion : memalign failure");
		return 0;
	}
	memset(rTMD,0, tmd_size );
	r = ES_GetTMDView(TitleID, rTMD, tmd_size);
	if(r<0)
	{
		gprintf("SysMenuVersion : GetTMDView error %d",r);
		mem_free( rTMD );
		return 0;
	}	
	sysver = rTMD->title_version;

	mem_free(rTMD);
	return sysver;
}

static bool DecryptvWiiSysMenu(void* data)
{
	dolhdr* dol = static_cast<dolhdr*>(data);

	// The ancast image will be in the first data section
	EspressoAncastHeader* anc = (EspressoAncastHeader*) ((u32) data + dol->offsetData[0]);

	// Verify the ancast image
	if (anc->header_block.magic != ANCAST_MAGIC)
	{
		gprintf("invalid ancast magic");
		return false;
	}

	if (anc->header_block.header_block_size != sizeof(AncastHeaderBlock))
	{
		gprintf("invalid ancast header block");
		return false;
	}

	if (anc->signature_block.signature_type != 0x01)
	{
		gprintf("invalid ancast signature type");
		return false;
	}

	if (anc->info_block.image_type != ANCAST_IMAGE_TYPE_ESPRESSO_WII)
	{
		gprintf("invalid ancast image type");
		return false;
	}

	// Verify body hash
	u32 hash[5] ATTRIBUTE_ALIGN(32) = {};
	SHA_Init();
	if (SHA_Calculate(reinterpret_cast<u8 *>(anc + 1), anc->info_block.body_size, hash) < 0)
	{
		gprintf("failed to calculate ancast hash");
		return false;
	}
	if (memcmp(anc->info_block.body_hash, hash, sizeof(hash)) != 0)
	{
		gprintf("invalid ancast hash");
		return false;
	}
	SHA_Close();

	// Initialize the AES engine
	if (AES_Init() < 0)
	{
		gprintf("failed to init aes");
		return false;
	}

	// Decrypt the ancast body
	static const u8 vwii_ancast_retail_key[0x10] = { 0x2e, 0xfe, 0x8a, 0xbc, 0xed, 0xbb, 0x7b, 0xaa, 0xe3, 0xc0, 0xed, 0x92, 0xfa, 0x29, 0xf8, 0x66 };
	static const u8 vwii_ancast_devel_key[0x10] = { 0x26, 0xaf, 0xf4, 0xbb, 0xac, 0x88, 0xbb, 0x76, 0x9d, 0xfc, 0x54, 0xdd, 0x56, 0xd8, 0xef, 0xbd };
	static u8 vwii_ancast_iv[0x10] = { 0x59, 0x6d, 0x5a, 0x9a, 0xd7, 0x05, 0xf9, 0x4f, 0xe1, 0x58, 0x02, 0x6f, 0xea, 0xa7, 0xb8, 0x87 };

	int ret;
	if (anc->info_block.console_type == ANCAST_CONSOLE_TYPE_RETAIL)
	{
		ret = AES_Decrypt(vwii_ancast_retail_key, sizeof(vwii_ancast_retail_key), vwii_ancast_iv, sizeof(vwii_ancast_iv), anc + 1, anc + 1, anc->info_block.body_size);
	}
	else if (anc->info_block.console_type == ANCAST_CONSOLE_TYPE_DEV)
	{
		ret = AES_Decrypt(vwii_ancast_devel_key, sizeof(vwii_ancast_devel_key), vwii_ancast_iv, sizeof(vwii_ancast_iv), anc + 1, anc + 1, anc->info_block.body_size);
	}
	else
	{
		gprintf("invalid ancast console type");
		return false;
	}

	if (ret < 0)
	{
		gprintf("failed to decrypt ancast body");
		return false;
	}

	AES_Close();
	return true;
}

static void PatchvWiiSysMenu(u8* mem_block, u32 max_address)
{
	// These are necessary builtin patches to the vWii System Menu to make it boot with Priiloader

	// Patch out IPC wait
	static u32 ipc_wait_hash[] = { 0x546007bd, 0x4182fff4 };
	static u32 ipc_wait_patch[] = { 0x546007bd, 0x60000000 };
	for(u32 add = 0; add + (u32)mem_block < max_address; add++)
	{
		if (!memcmp(mem_block + add, ipc_wait_hash, sizeof(ipc_wait_hash)))
		{
			memcpy(mem_block + add, ipc_wait_patch, sizeof(ipc_wait_patch) );
			DCFlushRange((u8 *)((add+(u32)mem_block) >> 5 << 5), (sizeof(ipc_wait_patch) >> 5 << 5) + 64);
			break;
		}
	}

	// ES_OpenActiveTitleContent(1 -> 9)
	// This is necessary since what was previously content 1 is now priiloader
	// and the original content 1 was moved to the end (which is now 9)
	static u32 active_content_hash_jp[] = { 0x38600001, 0x482ad159 };
	static u32 active_content_hash_us[] = { 0x38600001, 0x482836f9 };
	static u32 active_content_hash_eu[] = { 0x38600001, 0x48283785 };
	static u32 active_content_patch[] = { 0x38600009 };
	const u32* active_content_hash;
	u32 active_content_hash_size;
	switch (GetSysMenuVersion() & 0xf)
	{
	case 0: // jp
		active_content_hash = active_content_hash_jp;
		active_content_hash_size = sizeof(active_content_hash_jp);
		break;
	case 1: // us
		active_content_hash = active_content_hash_us;
		active_content_hash_size = sizeof(active_content_hash_us);
		break;
	case 2: // eu
		active_content_hash = active_content_hash_eu;
		active_content_hash_size = sizeof(active_content_hash_eu);
		break;
	default:
		error = ERROR_SYSMENU_GENERAL;
		throw "invalid system menu region";
	}

	for(u32 add = 0; add + (u32)mem_block < max_address; add++)
	{
		if (!memcmp(mem_block + add, active_content_hash, active_content_hash_size))
		{
			memcpy(mem_block + add, active_content_patch, sizeof(active_content_patch) );
			DCFlushRange((u8 *)((add+(u32)mem_block) >> 5 << 5), (sizeof(active_content_patch) >> 5 << 5) + 64);
			break;
		}
	}

	// nop out __VIInit since that's handled by BC-NAND now
	// Due to a VI bug? on vWii, letting SM call `__VIInit` again results in all sorts of issues
	// (black screens, green bars, freezes)
	static u32 vi_init_hash_jp[] = { 0x9421ffe0, 0x7c0802a6, 0x3c80816b };
	static u32 vi_init_hash_us_eu[] = { 0x9421ffe0, 0x7c0802a6, 0x3c808168 };
	static u32 vi_init_patch[] = { 0x4e800020 };
	const u32* vi_init_hash;
	u32 vi_init_hash_size;
	switch (GetSysMenuVersion() & 0xf)
	{
	case 0: // jp
		vi_init_hash = vi_init_hash_jp;
		vi_init_hash_size = sizeof(vi_init_hash_jp);
		break;
	case 1: // us
	case 2: // eu
		vi_init_hash = vi_init_hash_us_eu;
		vi_init_hash_size = sizeof(vi_init_hash_us_eu);
		break;
	default:
		error = ERROR_SYSMENU_GENERAL;
		throw "invalid system menu region";
	}

	for(u32 add = 0; add + (u32)mem_block < max_address; add++)
	{
		if (!memcmp(mem_block + add, vi_init_hash, vi_init_hash_size))
		{
			memcpy(mem_block + add, vi_init_patch, sizeof(vi_init_patch) );
			DCFlushRange((u8 *)((add+(u32)mem_block) >> 5 << 5), (sizeof(vi_init_patch) >> 5 << 5) + 64);
			break;
		}
	}
}

void BootMainSysMenu( void )
{	
	//boot info
	s8* ticket = NULL;

	//loader & boot file
	void* binary = NULL;
	u32 bootfile_size = 0;
	u8* patch_ptr = NULL;
	void* loader_addr = NULL;
	loader_t loader = NULL;

	//general
	s32 ret = 0;
	s32 fd = 0;
	try
	{
		auto TitleInfo = std::make_unique<TitleInformation>(SYSTEMMENU_TITLEID);
		//get the TMD. we need the TMD, not views, as SM 1.0's boot index != last content
		auto smTmd = TitleInfo->GetTMD();
		gdprintf("ios version: %u",(u8)smTmd->sys_version);

		if (smTmd->boot_index > smTmd->num_contents)
		{
			error = ERROR_SYSMENU_GETTMDFAILED;
			throw ("Invalid boot index " + std::to_string(smTmd->boot_index));
		}

		//get main.dol filename
		char file[64] ATTRIBUTE_ALIGN(32);
		u32 fileID = smTmd->contents[smTmd->boot_index].cid;
		gprintf("using %08X for booting",fileID);
		if( fileID == 0 )
		{
			error = ERROR_SYSMENU_BOOTNOTFOUND;
			throw "boot file not found";
		}

		// installing priiloader renamed system menu so we change the app file to have the right name
		fileID = 0x10000000 | (fileID & 0x0FFFFFFF);
		sprintf( file, "/title/00000001/00000002/content/%08x.app", fileID );

		fd = ISFS_Open( file, ISFS_OPEN_READ );
		if( fd < 0 )
		{
			error = ERROR_SYSMENU_BOOTOPENFAILED;
			throw "error opening " + std::to_string(fileID) + ".app : errno " + std::to_string(fd);
		}

		STACK_ALIGN(fstats, status, 1, 32);
		memset( status, 0, sizeof(fstats) );
		ret = ISFS_GetFileStats( fd, status);
		if( ret < 0 || status->file_length == 0)
		{
			error = ERROR_SYSMENU_BOOTGETSTATS;
			throw "retrieving bootfile stats failed";
		}
		bootfile_size = status->file_length;

		binary = mem_align(32, ALIGN32(bootfile_size) );
		if(!binary)
		{
			error = ERROR_MALLOC;
			throw "binary allocation failed";
		}

		if(ISFS_Read( fd, binary, bootfile_size) < (s32)bootfile_size)
		{
			error = ERROR_SYSMENU_BOOTREADFAILED;
			throw "failed to read binary";
		}
		ISFS_Close(fd);

		// If this is a vWii menu, it needs to be decrypted first
		if (smTmd->vwii_title)
		{
			gprintf("decrypting vwii sysmenu");
			if (!DecryptvWiiSysMenu(binary))
			{
				error = ERROR_SYSMENU_GENERAL;
				throw "failed to decrypt vwii menu";
			}
		}

		gprintf("loading hacks");
		LoadSystemHacks(StorageDevice::NAND);

		Input_Shutdown(false);
		ShutdownMounts();
		USB_Deinitialize();
		gprintf("subsystems shutdown");

		//Step 1 of IOS handling : Reloading IOS if needed;
		if( !SGetSetting( SETTING_USESYSTEMMENUIOS ) )
		{
			s32 ToLoadIOS = SGetSetting(SETTING_SYSTEMMENUIOS);
			if ( ToLoadIOS != (u8)IOS_GetVersion() )
			{
				if (IsIOSstub(ToLoadIOS) )
				{
					Input_Init();
					error=ERROR_SYSMENU_IOSSTUB;
					throw "ios stub";
				}

				ISFS_Deinitialize();
				ReloadIOS(ToLoadIOS, 1);
				ISFS_Initialize();
				system_state.ReloadedIOS = 1;

				// Any IOS < 28 does not have to required ES calls to get a title TMD, which sucks.
				// Therefor we will patch in NAND Access so we can load the TMD directly from nand.
				// Nasty fix, but this fixes System menu 1.0, which uses IOS9
				if (ToLoadIOS < 28)
				{
					gprintf("IOS < 28, patching in NAND Access");
					PatchIOS({ NandAccessPatcher });
				}
			}
		}
		/*
		//technically if we want to use sys menu IOS but its not loaded we need to reload, but i fail to see why
		else if ((u8)IOS_GetVersion() != (u8)rTMD->sys_version)
		{
			gprintf("Use system menu is ON, but IOS %d isn't loaded. reloading IOS...",(u8)rTMD->sys_version);
			__ES_Close();
			__IOS_ShutdownSubsystems();
			__ES_Init();
			__IOS_LaunchNewIOS ( (u8)rTMD->sys_version );
			__ES_Init();

			gprintf("launched ios %d for system menu",IOS_GetVersion());
			system_state.ReloadedIOS = 1;
		}*/

		//Step 2 of IOS handling : ES_Identify if we are on a different ios or if we reloaded ios once already. note that the ES_Identify is only supported by ios > 20
		if (((u8)IOS_GetVersion() != (u8)smTmd->sys_version) || (system_state.ReloadedIOS) )
		{
			if (system_state.ReloadedIOS)
				gprintf("Forced into ES_Identify");
			else
				gprintf("Forcing ES_Identify");

			//read ticket from FS
			fd = ISFS_Open("/title/00000001/00000002/content/ticket", 1 );
			if( fd < 0 )
			{
				error = ERROR_SYSMENU_TIKNOTFOUND;
				throw "failed to open ticket";
			}

			//get size
			memset( status, 0, sizeof(fstats) );
			if( ISFS_GetFileStats( fd, status ) < 0 )
			{
				error = ERROR_SYSMENU_TIKSIZEGETFAILED;
				throw "failed to get ticket size";
			}

			//create buffer
			ticket = static_cast<s8*>(mem_align( 32, ALIGN32(status->file_length) ));
			if( ticket == NULL )
			{
				error = ERROR_MALLOC;
				throw "failed to allocate ticket";
			}
			memset(ticket, 0, status->file_length );

			//read file
			ret = ISFS_Read( fd, ticket, status->file_length );
			if( ret < 0 )
			{
				error = ERROR_SYSMENU_TIKREADFAILED;
				throw ("failed to read ticket -> errno " + std::to_string(ret));
			}
			ISFS_Close( fd );

			fd = ISFS_Open("/sys/cert.sys",ISFS_OPEN_READ);
			if(fd < 0 )
			{
				error = ERROR_SYSMENU_ESDIVERFIY_FAILED;
				throw ("ES_Identify: ISFS_Open error " + std::to_string(ret));
			}

			STACK_ALIGN(fstats, certStats, 1, 32);
			memset( certStats, 0, sizeof(fstats) );
			ret = ISFS_GetFileStats( fd, certStats);
			if( ret < 0 || certStats->file_length == 0)
			{
				error = ERROR_SYSMENU_ESDIVERFIY_FAILED;
				throw ("ES_Identify: ISFS_GetFileStats error " + std::to_string(ret));
			}

			STACK_ALIGN(u8, certificate, certStats->file_length, 32);
			memset( certificate, 0, certStats->file_length );
			ret = ISFS_Read(fd, certificate, certStats->file_length);
			if(ret < 0 || (u32)ret < certStats->file_length)
			{
				error = ERROR_SYSMENU_ESDIVERFIY_FAILED;
				throw ("ES_Identify: ISFS_Read error " + std::to_string(ret));
			}

			//attempt to patch ESIdentify & Fakesign. we adjusted the SM TMD, so fakesign is needed to let ES accept it
			PatchIOS({FakeSignPatch, FakeSignOldPatch, EsIdentifyPatch});
			auto signedBlob = TitleInfo->GetRawTMD();
			ret = ES_Identify( reinterpret_cast<signed_blob *>(certificate), certStats->file_length, signedBlob, SIGNED_TMD_SIZE(signedBlob), reinterpret_cast<signed_blob *>(ticket), status->file_length, 0);
			if (ret < 0)
			{	
				error=ERROR_SYSMENU_ESDIVERFIY_FAILED;
				throw ("ES_Identify error " + std::to_string(ret));
			}

			if(ticket)
				mem_free(ticket);
		}

		//ES_SetUID(TitleID);

		gprintf("Hacks:%d",system_hacks.size());
		u8* mem_block = static_cast<u8*>(binary);
		u32 max_address = (u32)(mem_block + bootfile_size);
		u32 size = 0;
		u32 patch_cnt = 0;
		
		// Loop over sub-hacks to see which to enable and which's master to enable
		for(u32 i = 0;i < system_hacks.size();i++)
		{
			if (system_hacks[i].requiredID.length() == 0 || states_hash[i] == 0)
				continue;

			s32 masterIndex = GetMasterHackIndexByID(system_hacks[i].requiredID);

			//did we find the hack?
			//if so, enable it. else , disable the sub hack
			if ( masterIndex >= 0 )
				states_hash[masterIndex] = 1;
			else
				states_hash[i] = 0;
		}

		//get the size we need for the offset hacks
		//i know a regular for loop looks better, but this is HELLA faster. like, 170ms vs 6ms faster
		//we sadly can't put the previous loop into this cause we could have looped over the master hack and then realize we need it...
		u32 index = 0;
		std::for_each(system_hacks.begin(), system_hacks.end(), [&index,&size](const system_hack& obj)
		{
			if(states_hash[index++] == 0)
				return 0;

			std::for_each(obj.patches.begin(), obj.patches.end(), [&size](const system_patch _patch)
			{
				if(_patch.offset <= 0 || _patch.patch.size() <= 0 )
					return 0;
				size += 8 + _patch.patch.size();
				return 1;
			});
			
			return 1;
		});
		
		if(size > 0)
		{
			patch_ptr = static_cast<u8*>(mem_align(32,size));
			if(patch_ptr == NULL)
				throw "failed to malloc memory for patches";
		}
		size = 0;
		
		for(u32 i = 0;i < system_hacks.size();i++)
		{
			if(states_hash[i] != 1)
				continue;

			gprintf("applying %s",system_hacks[i].desc.c_str());
			u32 add = 0;
			for(u32 y = 0; y < system_hacks[i].patches.size();y++)
			{
				if(system_hacks[i].patches[y].patch.size() <= 0)
					continue;

				//offset method		
				//we copy these to mem2 for the loader to apply
				if(system_hacks[i].patches[y].offset > 0 && patch_ptr != NULL)
				{
					offset_patch* patch = reinterpret_cast<offset_patch*>(patch_ptr + size);
					size += 8 + system_hacks[i].patches[y].patch.size();

					patch->offset = system_hacks[i].patches[y].offset;
					patch->patch_size = system_hacks[i].patches[y].patch.size();

					for(u32 z = 0;z < system_hacks[i].patches[y].patch.size(); z++)
					{
						patch->patch[z] = system_hacks[i].patches[y].patch[z];
					}

					DCFlushRange(patch, system_hacks[i].patches[y].patch.size()+8);
					patch_cnt++;
					gprintf("added offset to list 0x%08X",patch->offset);
					continue;
				}
				//hash method
				else if(system_hacks[i].patches[y].hash.size() > 0)
				{
					u8 temp_hash[system_hacks[i].patches[y].hash.size()];
					u8 temp_patch[system_hacks[i].patches[y].patch.size()];
					memset(temp_hash, 0, system_hacks[i].patches[y].hash.size());
					memset(temp_patch, 0, system_hacks[i].patches[y].patch.size());
					std::copy(system_hacks[i].patches[y].hash.begin(), system_hacks[i].patches[y].hash.end(), temp_hash);
					std::copy(system_hacks[i].patches[y].patch.begin(), system_hacks[i].patches[y].patch.end(), temp_patch);

					while( add + (u32)mem_block < max_address)
					{
						if ( !memcmp(mem_block+add, temp_hash ,sizeof(temp_hash)) )
						{
							gprintf("Found %s @ 0x%X, patching hash # %d...",system_hacks[i].desc.c_str(), add, y+1);
							memcpy(mem_block+add,temp_patch,sizeof(temp_patch) );
							DCFlushRange((u8 *)((add+(u32)mem_block) >> 5 << 5), (sizeof(temp_patch) >> 5 << 5) + 64);
							break;
						}
						add++;
					}//end hash while loop
				}//end of hash or offset check
			} //end for loop of all patches of hack[i]
		} // end general hacks loop*/

		// Apply vWii specific patches which are always required for the menu to boot from priiloader
		if (smTmd->vwii_title)
		{
			gprintf("applying vWii patches");

			PatchvWiiSysMenu(mem_block, max_address);
		}

		//prepare loader
		loader_addr = static_cast<void*>(mem_align(32,loader_bin_size));
		if(!loader_addr)
			throw "failed to alloc the loader";

		memcpy(loader_addr,loader_bin,loader_bin_size);	
		DCFlushRange(loader_addr, loader_bin_size);
		ICInvalidateRange(loader_addr, loader_bin_size);
		loader = (loader_t)loader_addr;

		if(system_state.Init)
		{
			VIDEO_SetBlack(true);
			VIDEO_Flush();
			VIDEO_WaitVSync();
		}

		__STM_Close();
		__IPC_Reinitialize();
		__IOS_ShutdownSubsystems();
		IRQ_Disable();
		gprintf("launching sys menu... 0x%08X",loader_addr);
		
		//loader
		ICSync();
		const u8 binaryType = smTmd->vwii_title ? BINARY_TYPE_SYSTEM_MENU_VWII : BINARY_TYPE_SYSTEM_MENU;
		loader(binary, patch_ptr, patch_cnt, binaryType);
		gprintf("this ain't good");

		//oh ow, this ain't good
		if(system_state.Init)
		{
			VIDEO_SetBlack(false);
			VIDEO_Flush();
			VIDEO_WaitVSync();
		}
	}
	catch (const std::string& ex)
	{
		gprintf("BootMainSysMenu Exception -> %s",ex.c_str());
	}
	catch (char const* ex)
	{
		gprintf("BootMainSysMenu Exception -> %s",ex);
	}
	catch(...)
	{
		gprintf("BootMainSysMenu Exception was thrown");
	}

	if (patch_ptr)
		mem_free(patch_ptr);
	if (loader_addr)
		mem_free(loader_addr);
	if(ticket)
		mem_free(ticket);
	if(binary)
		mem_free(binary);
	if(fd > 0)
		ISFS_Close( fd );
	Input_Init();
	InitMounts(_mountCallback);

	return;
}