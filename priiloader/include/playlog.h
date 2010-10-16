#ifndef _PLAYLOG_H_
#define _PLAYLOG_H_

#ifdef __cplusplus
#define PLAYRECPATH "/title/00000001/00000002/data/play_rec.dat"

extern "C" {
#endif /* __cplusplus */

/* Prototypes */
u64 getWiiTime(void);
int Playlog_Update(const char ID[6], const u8 title[84]);
int Playlog_Delete(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

