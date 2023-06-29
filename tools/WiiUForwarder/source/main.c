#include <stdlib.h>
#include <malloc.h>
#include <whb/proc.h>
#include <padscore/kpad.h>
#include <nn/cmpt.h>
#include <coreinit/thread.h>

#define MAGIC_TITLE_ID_DACO 0x505249494461636fllu // 'PRII-Daco'

int main(int argc, char** argv)
{
    WHBProcInit();

    // KPAD needs to be initialized for cmpt
    KPADInit();

    // Try to find a screen type that works
    CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_BOTH);
    if (CMPTCheckScreenState() < 0)
    {
        CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_DRC);
        if (CMPTCheckScreenState() < 0)
        {
            CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_TV);
        }
    }

    uint32_t dataSize = 0;
    CMPTGetDataSize(&dataSize);

    void *dataBuffer = memalign(0x40, dataSize);

    // Launch (non-existant) magic title
    CMPTLaunchTitle(dataBuffer, dataSize, MAGIC_TITLE_ID_DACO);

    while (WHBProcIsRunning())
    {
        OSYieldThread();
    }

    free(dataBuffer);

    WHBProcShutdown();

    return 0;
}
