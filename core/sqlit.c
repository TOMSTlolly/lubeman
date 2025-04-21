#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>
#include "comutils.h"
#include "WinTypes.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"


#define MAXBUF 256
static sqlite3 *db;
char *zErrMsg = 0;


static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
	int i;
	for (i = 0; i < argc; i++) {
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	printf("\n");
	return 0;
}


char do_sql(char *sql)
{
	int rc;	
	rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	
	rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL: %s\r\n", sql);
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		return (0);
	}
	else {
		//fprintf(stdout, "\r\nRecord %s created successfully\n",sql);
		rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);

		return (1);
	}
}

int update_sql(char *sql)
{
	sqlite3_stmt *stmt;
	//const char* data = "Callback function called";
	int rc;

	/*
	close_database();
	rc = sqlite3_open("./test.db", &db);
   
	if (rc) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		return (0);
	}
	else {
		fprintf(stderr, "Opened database successfully\n");
	}
	;
	*/
	
	rc = sqlite3_exec(db, sql, callback, 0,&zErrMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	else {
		fprintf(stdout, "Operation done successfully\n");
	}
	return (1);
}


int get_max(char *maxfield, char *table)
{
	sqlite3_stmt *stmt;
	char sql[MAXBUF];
	int rc;
	
	sprintf(sql, "select max(%s) as maxid from %s;", maxfield, table);

	//sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (SQLITE_OK != rc) {
		//fprintf(stderr, "Can't prepare insert statment %s (%i): %s\n", sql, rc, sqlite3_errmsg(db));
		printf("Can't prepare insert statment %s (%i): %s\n", sql, rc, sqlite3_errmsg(db));
		sqlite3_close(db);
		return -1;
	}
	//sqlite3_finalize(stmt);
	
	while (sqlite3_step(stmt) != SQLITE_DONE) {
		int i;
		int num_cols = sqlite3_column_count(stmt);
		printf("column count=%d\r",num_cols);

		i = sqlite3_column_int(stmt, 0);
		//printf ("i=%d\r\n",i);
		sqlite3_finalize(stmt);

		return (i);
	}
	sqlite3_finalize(stmt);
	return (0);
}

int get_id(int idSyn, char *table, int *ffree)
{
	sqlite3_stmt *stmt;
	char sql[MAXBUF];

	sprintf(sql, "select rowid,firstfree from %s where idSyn=%d", table, idSyn);

	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	while (sqlite3_step(stmt) != SQLITE_DONE) {
		int i;
		int num_cols = sqlite3_column_count(stmt);
		//printf("column count=%d\r",num_cols);

		i = sqlite3_column_int(stmt, 0);  // v nultem columnu je id (primarni klic)
		*ffree = sqlite3_column_int(stmt, 1); //

		//printf ("i=%d\r\n",i);
		return (i);
	}
	return (-1);
}

// vezme posledni zaznam, podiva se na tok(TransferOK), pokud je tok<1, vraci idsyn zaznam pro otevreni blobu
int untransfered_id(char *table, int *tok, WORD *fif)
{
	sqlite3_stmt *stmt;
	char sql[MAXBUF];

	sprintf(sql, "select max(idsyn) as max, firstfree, tok from %s", table);
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	while (sqlite3_step(stmt) != SQLITE_DONE) {
		int max;
		int num_cols = sqlite3_column_count(stmt);
		max          = sqlite3_column_int(stmt, 0);   // v nultem columnu je posledni idSyn
		*fif         = sqlite3_column_int(stmt, 1);
		*tok         = sqlite3_column_int(stmt, 2); 	 // transfer OK
		printf ("max=%d Transfer OK=%d\r\n",max,*tok);
		
		//*firstfree = fif;
		
		return (max);
	}
	return (-1);
}

// ulozi prave naplneny blob do souboru
int save_blob_to_file(UCHAR **pcBufRead,char *filename)
{
	FILE *pfile = NULL;
	//char *filename = "dump.bin";
	pfile = fopen(filename, "w");
	if (pfile == NULL)
	{
		printf("Error opening %s for writing. Program terminated.", filename);
	}

	char c;
	for (int i = 0; i < MAXBUF; i++)
	{
		//pcBufRead[i] = i;
		printf("%2x ", pcBufRead[i]);
		putc((int)pcBufRead[i], pfile);
	}
	fclose(pfile);
	printf("\r\n\r\n");
}

int get_firstfree(char *table, int idSyn)
{
	sqlite3_stmt *stmt;
	char sql[MAXBUF];

	sprintf(sql, "select firstfree from %s", table);
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	while (sqlite3_step(stmt) != SQLITE_DONE) {
		//int num_cols = sqlite3_column_count(stmt);
		int firstfree    = sqlite3_column_int(stmt, 0);   // v nultem columnu je posledni idSyn
		printf ("firstFree=%d \r\n",firstfree);
		return (firstfree);
	}
	return (-1);
}

int get_sensorid(int synId)
{
	sqlite3_stmt *stmt;
	char sql[MAXBUF];

	sprintf(sql, "select sensor from repx where synId=%d and orden=1", synId);
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	while (sqlite3_step(stmt) != SQLITE_DONE) {
		//int num_cols = sqlite3_column_count(stmt);
		int sensor    = sqlite3_column_int(stmt, 0);   // v nultem columnu je posledni idSyn
		printf ("sensor=%d \r\n",sensor);
		return (sensor);
	}
	return (-1);
}

int read_blob(int idSyn, UCHAR **pcBufRead, int *blobsize, int *firstfree)
{
	//int firstfree;
	int ff=0;
	int rowid = get_id(idSyn, "rawbuf",&ff);
	*firstfree= ff;
	
	sqlite3_blob *blob;
	int rc = sqlite3_blob_open(db, "main","rawbuf", "buffer", rowid, 0, &blob);
	if (SQLITE_OK != rc) {
		fprintf(stderr, "Couldn't get blob handle (%i): %s\n", rc, sqlite3_errmsg(db));
		return(1);
	}
	//int blobsize
	*blobsize= sqlite3_blob_bytes(blob);
	//*firstfree = *blobsize;
	printf("sqlite_blob_bytes=%d rowid=%d synId=%d\r\n", blobsize, rowid, idSyn);

	unsigned char *buffer = malloc(*blobsize);
	if (buffer == NULL) {
		fprintf(stderr, "Cannot allocate memory\n");
		sqlite3_blob_close(blob);
		sqlite3_close(db);
		return 1;
	}


	rc = sqlite3_blob_read(blob, buffer, *blobsize, 0);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "Cannot read BLOB: %s\n", sqlite3_errmsg(db));
		free(buffer);
		sqlite3_blob_close(blob);
		sqlite3_close(db);
		return rc;
	}

//	save_blob_to_file(buffer, "dump.bin");

	*pcBufRead =  malloc(*blobsize);
	memcpy(*pcBufRead, buffer, *blobsize);
	/*
	for (int i =0 ; i<blobsize; i++) {
		printf("%2X \r\n", pcBufRead[i]);
		//*pcBufRead[i]=buffer[i];
	}
	*/
	dumpBuffer(*pcBufRead,*blobsize);

	/*
	int chunk = 100; 
	int iOffset = 0; 
	char *zBlob = (char *) malloc(chunk); 
	int ret = SQLITE_OK; 
	int j;
	j = 0;
	while (ret == SQLITE_OK) 
	{ 
		if ((iOffset + chunk) > blobsize) 
		{ 
			chunk = blobsize - iOffset; 
			if (chunk == 0) 
			{ 
				break; 
			} 
		} 
		ret = sqlite3_blob_read(blob, zBlob, chunk, iOffset); 
		printf("sqlite3_blob_read ret=%d chunk=%d\r\n", ret, chunk);
		//output.write(zBlob, chunk);
		for(int i = 0 ; i < chunk ; i++)
		{
			pcBufRead[j++] = zBlob[i];
			printf("%2X ", zBlob[i]);
		}
		iOffset += chunk; 
		printf("\r\n");
	}
	*/
	sqlite3_blob_close(blob);
	printf("\r\nblob finish\r\n");	
	save_blob_to_file(pcBufRead, "dump.bin");
}


int remedy(char *str, int *ifrom, int *into)
	{
		char *pch;
		pch = strtok(str, " "); 
  
		//printf ("pch=%s\r\n",*pch);
		int i = 0;
		while (pch != NULL)
		{
			if (i == 1)
				*ifrom = atoi(pch);
			else if (i == 2)
				*into = atoi(pch);

			//printf("%s\n", pch);
			pch = strtok(NULL, " ");  
			i++;  	
		}
		printf("atoi=%d into=%d\r\n", *ifrom, *into);
		return 0;
}

	

int new_file(WORD firstfree)
{
	char sql[256];
	sprintf(sql, "INSERT INTO gensyn(generated,cnt) VALUES (CURRENT_TIMESTAMP,%d)", firstfree);  
	do_sql(sql);

	// zjisti maximalni vlozeny id
	int synId = get_max("id", "gensyn");
	return (synId);
}

// write blob to database table "rawbuf"
int write_blob(unsigned char *pcBufRead, int firstfree, int idSyn)
{
	int rc;
	int minalo = MAXBUF;
	char tablename[]  = "rawbuf" ;
	char columnname[] = "buffer" ;
	char insert_sql[1024] ;
	snprintf(insert_sql, sizeof(insert_sql), "INSERT INTO %s (%s,idSyn,firstfree) VALUES (?,%d,%d)", tablename, columnname, idSyn,firstfree);
	//snprintf(insert_sql, sizeof(insert_sql), "INSERT INTO %s (%s) VALUES (?)", tablename, columnname);
	
	sqlite3_stmt *insert_stmt ;
	rc = sqlite3_prepare_v2(db, insert_sql, -1, &insert_stmt, NULL) ;
	if(SQLITE_OK != rc) {
		fprintf(stderr, "Can't prepare insert statment %s (%i): %s\n", insert_sql, rc, sqlite3_errmsg(db)) ;
		sqlite3_finalize(insert_stmt);
		sqlite3_close(db) ;
		exit(1) ;
	}
	
	// Bind a block of zeros the size of the file we're going to insert later
	sqlite3_bind_zeroblob(insert_stmt, 1, minalo) ;
	if (SQLITE_DONE != (rc = sqlite3_step(insert_stmt))) {
		fprintf(stderr, "Insert statement didn't work (%i): %s\n", rc, sqlite3_errmsg(db));
		sqlite3_finalize(insert_stmt);
		exit(1);
	}
	sqlite3_finalize(insert_stmt);
	
	sqlite3_int64 rowid = sqlite3_last_insert_rowid(db) ;
	//printf("Created a row, id %i, with a blank blob size %i\n", (int)rowid, (int)minalo) ;

	// Getting here means we have a valid file handle, f, and a valid db handle, db
	//   Also, a blank row has been inserted with key rowid
	sqlite3_blob *blob ;
	rc = sqlite3_blob_open(db, "main", tablename, columnname, rowid, 1, &blob) ;
	if(SQLITE_OK != rc) {
		fprintf(stderr, "Couldn't get blob handle (%i): %s\n", rc, sqlite3_errmsg(db)) ;
		exit(1) ;
	}

   if(SQLITE_OK != (rc = sqlite3_blob_write(blob, pcBufRead, minalo, 0))) {
		fprintf(stderr, "Error writing to blob handle. Offset %i, len %i\n", 0, minalo) ;		
   }
	sqlite3_blob_close(blob);
}



int close_database(void)
{
	//sqlite3_close(db);
	
	
	int i = 0,
	rc = 0;
	rc = sqlite3_close(db);
	while (rc != SQLITE_OK)
	{
		printf("yet closing\n");
		if (rc == SQLITE_BUSY)
		{
			printf("it is busy\n");
			i++;
			if (i > 10)
			{
				return rc;
			}
		}
		
		msleep(1);
		rc = sqlite3_close_v2(db);
	}
	printf("2closeeeeeee\n\n");
	return 0;

}

int open_database(char *dbname)
{
	sqlite3 **ppDb ;//= &db;
	int rc = sqlite3_open("test.db", &db);
	//&db = (volatile sqlite3) ppDb;
	if (rc != SQLITE_OK)
	//if (db == NULL)
	{
		logPrintf("Failed to open DB\n");
		return 0;
	}
	return 1;
}


