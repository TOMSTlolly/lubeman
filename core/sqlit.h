#ifndef SQLIT_H
#define SQLIT_H

#include "WinTypes.h"
#include <time.h>
#pragma schema.journal_mode =  OFF

int new_file(WORD firstfree);

int get_max(char *maxfield, char *table);

int get_id(int idSyn, char *table);

int close_database(void);


int open_database( char *dbname);
char do_sql(char *sql);

int write_blob(unsigned char *pcBufRead, int firstfree, int idSyn);

int read_blob(int idSyn,UCHAR **pcBufRead,int *blobsize, int *firstfree);

int get_sensorid(int synId);

int untransfered_id(char *table, int *tok, WORD *fif);

int update_sql(char *sql);

int remedy(char *str, int *ifrom, int *into);


#endif
