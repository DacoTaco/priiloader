/*
vWii.cpp - vWii compatibility utils for priiloader

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

#ifndef _VWII_H_
#define _VWII_H_

#include <gctypes.h>
#include <assert.h>

#define WIIU_MAGIC 0x57696955 // 'WiiU'

typedef struct {
	u8 ipl_ar;
	u8 ipl_e60;
	u8 ipl_pgs;
	u8 ipl_ssv;
	u8 ipl_snd;
	u8 __padding[3];
} WiiUAVConfig;
static_assert(sizeof(WiiUAVConfig) == 0x8);

typedef struct {
	u8 bt_dinf[0x461];
	u8 bt_mot;
	u8 bt_spkv;
	u8 bt_bar;
	u32 bt_sens;
} WiiURemoteConfig;
static_assert(sizeof(WiiURemoteConfig) == 0x468);

typedef struct {
	u8 enabled;
	u8 max_rating;
	u8 organization;
	u8 __padding[3];
	u8 rst_pt_order;
	u8 rst_nw_access;
	u8 rst_internet_ch;
	char pin[4];
	u8 secret_question;
	u16 answer_len;
	u16 answer[0x20];
} WiiUParentalConfig;
static_assert(sizeof(WiiUParentalConfig) == 0x50);

typedef struct {
	u32 magic;
	u32 ipl_cb;
	u32 ipl_apd;
	char ipl_nik[0x14];
	u16 ipl_nik_len;
	u8 ipl_lng;
	u8 region;
	u8 ipl_eula;
	u8 __padding[3];
	WiiUAVConfig av;
	WiiURemoteConfig remote;
	WiiUParentalConfig parental;
} WiiUConfig;
static_assert(sizeof(WiiUConfig) == 0x4E8);

typedef struct {
	uint32_t magic;
	uint32_t flags;
	uint64_t title_id;
	uint8_t reserved[0x70];
} WiiUArgs;
static_assert(sizeof(WiiUArgs) == 0x80);

bool CheckvWii(void);

void ImportWiiUConfig(void);

const WiiUConfig* GetWiiUConfig(void);

const WiiUArgs* GetWiiUArgs(void);

void SetWiiUArgs(const WiiUArgs* args);

s32 STM_DMCUWrite(u32 message);

#endif