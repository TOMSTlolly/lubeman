// System libraries like timeouts etc.

#ifndef COMUTILS_H
#define COMUTILS_H

#define LOWBYTE(v)   ((unsigned char) (v))
#define HIGHBYTE(v)  ((unsigned char) (((unsigned int) (v)) >> 8))
#include "WinTypes.h"
typedef unsigned char ChipIDDef[8];

typedef struct _evstamp
{
	BYTE second, minute, hour, day, month;
	WORD year;
} evstamp ;


typedef struct _evrepx
{
	evstamp evs;
	WORD orden;
	DWORD pessn;
	char chip[24];
	WORD typ, info;
	DWORD idSyn;
	
} evrepx;

void logLog(const char *fmt, ...);
#define logPrintf logLog

unsigned int GetTickCount();
void msleep(unsigned int asleep);
void dumpBuffer(unsigned char *buffer, int acount);
//BOOL p3_reconnect(void);
//BOOL _purgerxtx(void);
void clearbuffer(char *buffer, int acount);
unsigned char calc8(ChipIDDef ID);

#endif
