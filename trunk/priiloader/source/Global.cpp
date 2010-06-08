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
//global functions,defines & variables for priiloader
#include "Global.h"
GXRModeObj *rmode = NULL;
void *xfb = NULL;

static const struct _timing {
	u8 equ;
	u16 acv;
	u16 prbOdd,prbEven;
	u16 psbOdd,psbEven;
	u8 bs1,bs2,bs3,bs4;
	u16 be1,be2,be3,be4;
	u16 nhlines,hlw;
	u8 hsy,hcs,hce,hbe640;
	u16 hbs640;
	u8 hbeCCIR656;
	u16 hbsCCIR656;
} video_timing[] = {
	{
		0x06,0x00F0,
		0x0018,0x0019,0x0003,0x0002,
		0x0C,0x0D,0x0C,0x0D,
		0x0208,0x0207,0x0208,0x0207,
		0x020D,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x06,0x00F0,
		0x0018,0x0018,0x0004,0x0004,
		0x0C,0x0C,0x0C,0x0C,
		0x0208,0x0208,0x0208,0x0208,
		0x020E,0x01AD,
		0x40,0x47,0x69,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x05,0x011F,
		0x0023,0x0024,0x0001,0x0000,
		0x0D,0x0C,0x0B,0x0A,
		0x026B,0x026A,0x0269,0x026C,
		0x0271,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C,
		0x85,0x01A4
	},
	{
		0x05,0x011F,
		0x0021,0x0021,0x0002,0x0002,
		0x0D,0x0B,0x0D,0x0B,
		0x026B,0x026D,0x026B,0x026D,
		0x0270,0x01B0,
		0x40,0x4B,0x6A,0xAC,
		0x017C,
		0x85,0x01A4
	},
	{
		0x06,0x00F0,
		0x0018,0x0019,0x0003,0x0002,
		0x10,0x0F,0x0E,0x0D,
		0x0206,0x0205,0x0204,0x0207,
		0x020D,0x01AD,
		0x40,0x4E,0x70,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x06,0x00F0,
		0x0018,0x0018,0x0004,0x0004,
		0x10,0x0E,0x10,0x0E,
		0x0206,0x0208,0x0206,0x0208,
		0x020E,0x01AD,
		0x40,0x4E,0x70,0xA2,
		0x0175,
		0x7A,0x019C
	},
	{
		0x0C,0x01e0,
		0x0030,0x0030,0x0006,0x0006,
		0x18,0x18,0x18,0x18,
		0x040e,0x040e,0x040e,0x040e,
		0x041a,0x01ad,
		0x40,0x47,0x69,0xa2,
		0x0175,
		0x7a,0x019c
	},
	{
		0x0c,0x01e0,
		0x002c,0x002c,0x000a,0x000a,
		0x18,0x18,0x18,0x18,
		0x040e,0x040e,0x040e,0x040e,
		0x041a,0x01ad,
		0x40,0x47,0x69,0xa8,
		0x017b,
		0x7a,0x019c
	},
	{
		0x06,0x00F1,
		0x0018,0x0019,0x0001,0x0000,
		0x0C,0x0D,0x0C,0x0D,
		0x0208,0x0207,0x0208,0x0207,
		0x020D,0x01AD,
		0x40,0x47,0x69,0x9F,
		0x0172,
		0x7A,0x019C
	},
	{
		0x0C,0x01E0,
		0x0030,0x0030,0x0006,0x0006,
		0x18,0x18,0x18,0x18,
		0x040E,0x040E,0x040E,0x040E,
		0x041A,0x01AD,
		0x40,0x47,0x69,0xB4,
		0x0187,
		0x7A,0x019C
	}
};

//a copy of Libogc's __VIInit
static void ___VIInit( void )
{
	u32 cnt;
	u32 vi_mode,interlace,progressive;
	const struct _timing *cur_timing = NULL;
	vu16* const _viReg = (u16*)0xCC002000;

	if(rmode == NULL)
		rmode = VIDEO_GetPreferredMode(NULL);
	vi_mode = ((rmode->viTVMode>>2)&0x07);
	interlace = (rmode->viTVMode&0x01);
	progressive = (rmode->viTVMode&0x02);

	switch(vi_mode) {
		case VI_TVMODE_NTSC_INT:
			cur_timing = &video_timing[0];
			break;
		case VI_TVMODE_NTSC_DS:
			cur_timing = &video_timing[1];
			break;
		case VI_TVMODE_PAL_INT:
			cur_timing = &video_timing[2];
			break;
		case VI_TVMODE_PAL_DS:
			cur_timing = &video_timing[3];
			break;
		case VI_TVMODE_EURGB60_INT:
			cur_timing = &video_timing[0];
			break;
		case VI_TVMODE_EURGB60_DS:
			cur_timing = &video_timing[1];
			break;
		case VI_TVMODE_MPAL_INT:
			cur_timing = &video_timing[4];
			break;
		case VI_TVMODE_MPAL_DS:
			cur_timing = &video_timing[5];
			break;
		case VI_TVMODE_NTSC_PROG:
			cur_timing = &video_timing[6];
			break;
		case VI_TVMODE_NTSC_PROG_DS:
			cur_timing = &video_timing[7];
			break;
		case VI_TVMODE_DEBUG_PAL_INT:
			cur_timing = &video_timing[2];
			break;
		case VI_TVMODE_DEBUG_PAL_DS:
			cur_timing = &video_timing[3];
			break;
		default:
			break;
	}

	//reset the interface
	cnt = 0;
	_viReg[1] = 0x02;
	while(cnt<1000) cnt++;
	_viReg[1] = 0x00;

	// now begin to setup the interface
	_viReg[2] = ((cur_timing->hcs<<8)|cur_timing->hce);		//set HCS & HCE
	_viReg[3] = cur_timing->hlw;							//set Half Line Width

	_viReg[4] = (cur_timing->hbs640<<1);					//set HBS640
	_viReg[5] = ((cur_timing->hbe640<<7)|cur_timing->hsy);	//set HBE640 & HSY

	_viReg[0] = cur_timing->equ;

	_viReg[6] = (cur_timing->psbOdd+2);							//set PSB odd field
	_viReg[7] = (cur_timing->prbOdd+((cur_timing->acv<<1)-2));	//set PRB odd field

	_viReg[8] = (cur_timing->psbEven+2);						//set PSB even field
	_viReg[9] = (cur_timing->prbEven+((cur_timing->acv<<1)-2));	//set PRB even field

	_viReg[10] = ((cur_timing->be3<<5)|cur_timing->bs3);		//set BE3 & BS3
	_viReg[11] = ((cur_timing->be1<<5)|cur_timing->bs1);		//set BE1 & BS1

	_viReg[12] = ((cur_timing->be4<<5)|cur_timing->bs4);		//set BE4 & BS4
	_viReg[13] = ((cur_timing->be2<<5)|cur_timing->bs2);		//set BE2 & BS2

	_viReg[24] = (0x1000|((cur_timing->nhlines/2)+1));
	_viReg[25] = (cur_timing->hlw+1);

	_viReg[26] = 0x1001;		//set DI1
	_viReg[27] = 0x0001;		//set DI1
	_viReg[36] = 0x2828;		//set HSR

	if(vi_mode<VI_PAL && vi_mode>=VI_DEBUG_PAL) vi_mode = VI_NTSC;
	if(progressive){
		_viReg[1] = ((vi_mode<<8)|0x0005);		//set MODE & INT & enable
		_viReg[54] = 0x0001;
	} else {
		_viReg[1] = ((vi_mode<<8)|(interlace<<2)|0x0001);
		_viReg[54] = 0x0000;
	}
}
void InitVideo ( void )
{
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
	gprintf("resolution is %dx%d\n",rmode->viWidth,rmode->viHeight);
}
//the following doesn't work cause almost no compilers supports export ;_;
/*export template <class pointer>
s8 free_null_pointer(pointer*& ptr)*/
s8 free_null_pointer(void*& ptr)
{
	if(ptr != NULL)
	{
		free(ptr); 
		ptr = NULL;
		return 0;
	}
	else
	{
		return -1;
	}
}

void Control_VI_Regs ( u8 mode )
{
	vu16* const _viReg = (u16*)0xCC002000;
	u32 cnt = 0;
	switch(mode)
	{
		case 0:
			if((_viReg[1]&0x0001))
			{
				//reset & de-init the regs. at least that should work according to libogc
				u32 cnt = 0;
				_viReg[1] = 0x02;
				while(cnt<1000) cnt++;
				_viReg[1] = 0x00;
			}
			break;
		case 1:
			//init VI Registers for anything needing them...like SI
			if(!(_viReg[1]&0x0001))
				___VIInit();
			while(cnt<1000) cnt++;
			break;
		case 2:
			if((_viReg[1]&0x0001))
			{
				//reset VI
				_viReg[1] = 0x02;
				while(cnt<1000) cnt++;
			}
			break;
		default:
			break;

	}
}

s8 InitNetwork()
{
	s32 result;
	gprintf("Waiting for network to initialise...\n");
    while ((result = net_init()) == -EAGAIN);
    if (result >= 0) 
	{
        char myIP[16];
		if (if_config(myIP, NULL, NULL, true) < 0) 
		{
			gprintf("Error reading IP address\n");
			sleep(5);
			return -1;
		}
        gprintf("Wii 's IP Address is %s\n",myIP);
		return 1;
    } 
	else 
	{
		gprintf("Unable to initialise network, no router in range or no DHCP timed out\n");
		sleep(5);
		return -2;
    }
}
