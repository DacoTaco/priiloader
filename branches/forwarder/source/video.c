#include <stdio.h>
#include <ogcsys.h>


static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void initVideoSubsys() {
        //do video init first so we can see crash screens
        VIDEO_Init();


        rmode = VIDEO_GetPreferredMode(NULL);


        //apparently the video likes to be bigger then it actually is on NTSC/PAL60/480p. lets fix that!
        if( rmode->viTVMode == VI_NTSC || rmode->viTVMode == VI_EURGB60 || CONF_GetProgressiveScan() )
        {
                //the correct one would be * 0.035 to be sure to get on the Action safe of the screen. but thats way to much
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
}
