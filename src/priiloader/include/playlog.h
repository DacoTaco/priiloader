#ifndef _PLAYLOG_H_
#define _PLAYLOG_H_

#define PLAYRECPATH "/title/00000001/00000002/data/play_rec.dat"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Prototypes */
u64 getWiiTime(void);
int Playlog_Update(const char ID[6], const unsigned char title[84]);
int Playlog_Delete(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

