#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>
#include "include/SimpleIni.h"
#include "cser.hh"
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <algorithm>
//#include "asmjs.h"

using namespace std;

extern "C" {
  #include "WinTypes.h"
  #include "engine.h"
  #include "parser.h"
  #include "comutils.h"
  #include "sqlit.h"
  #include "asmjs.h"
  #include "string.h"
  #include "tcppes.h"
}

int _auto_user;
char _host[16];
int _port;


#define DEBUG
#define IPCKEY 44
#define IPCRD  45
#define SATCNT 2   // pocet pokusu o spojeni se serverem
#define WAIT_FOR_IPC 2200 // pocet ms kdy cekam na odpoved ze shared memory IPC
#define WAIT_CHIP_TOUCH 5000 // spust cekacku na sonde
#define DELETE_ATT 5   // pocet pokusu o reconnect pri delete, pri neuspechu znovu na zacatek
#define SYNCHROTIMEWAIT 10
#define RMD_SECONDS 60  // po teto dobe odeslu packet na odeslani chybejicich dat .... 

int msqid = 0;
int msqrd = 0;

static int blink_interval_us = 500000;
pthread_t _blinkFork;  // pid_t _blinkFork;
static int _blinkState = 0;
static int _dbopen = 0;
static int _idSyn = -1;
static bool _isResend = false;
static char _line[256];

char fAdapterUser[16];
evrepx _evt;

std::vector<std::string> fts;

void Csr::setMachine(machine_state Astate)
{
	fMachineState =  Astate;
}

bool Csr::create_pipe(void)
{
	key_t key = IPCKEY;
	if ((msqid = msgget(key, IPC_CREAT | 0666)) < 0)
	{
		logPrintf("create_pipe ERROR");
		return (false);	
	}
	
	key = IPCRD;
	if ((msqrd = msgget(key, IPC_CREAT | 0666)) < 0)
	{
		logPrintf("create_pipe ERROR");
		return (false);	
	}
	return (true);	
}

bool Csr::put_msg(const char *msg)
{
  size_t buflen;
  BOOL ret;
  int i;

  fsbuf.mtype =1 ;
  sprintf(fsbuf.mtext,msg);
  buflen=strlen(fsbuf.mtext)+1;
  i = msgsnd(msqid, &fsbuf, buflen, IPC_NOWAIT);
  return (i >= 0);
	
}

bool Csr::wrt_read(const char* ainput, char *aoutput)
{
	if (put_msg(ainput) == false)
		return (false);
	
	msleep(500);
	DWORD StartTime = GetTickCount();
	int ret = 0;
	do
	{
	  aoutput[0] = 0;
	  ret = get_msg(aoutput);       
	  msleep(200);
	} while ( (ret<0) && (GetTickCount() - StartTime < 600) );
	
	// je to ta sama hodnota vyzvednuta z fronty
	ret = strcmp(ainput, aoutput);
	if (ret == 0)
		logPrintf("Rovnost \r\n");
	
	return (ret);
}

int Csr::get_msg(char *msoutput) 
{ 
	typedef struct msgbuf {
		long    mtype;
		char    mtext[MSGSZ];
	} message_buf;
	
	int ret;
	int msgtype = 1;   // 1..send, 2..read
	fsbuf.mtext[0] = 0;
	ret =  msgrcv(msqid, (void *) &fsbuf, 128, 1, 0);
	if (ret == -1)
	{ 
		/*
		if (errno != ENOMSG)
			return(EXIT_FAILURE - 10);
		
		return(EXIT_SUCCESS - 10);
		*/
	}
	
	printf("message received: %s\n", fsbuf.mtext);
	strcpy(msoutput, fsbuf.mtext);
	return (1);
}

void Csr::clr_buffer(char *val, char size)
{
	for (int i = 0; i < size; i++)
		val[i] = 0;
}

bool  Csr::test_msg(const char* stim)
{
	int ret = 0;
	
	typedef struct msgbuf {
		long    mtype;
		char    mtext[MSGSZ];
	} msg_buf;
	msg_buf  rbuf;

	rbuf.mtype = 2;
	clr_buffer(rbuf.mtext, MSGSZ);
	strcpy(rbuf.mtext, stim);
	ret = strlen(rbuf.mtext);
#ifdef DEBUG
	//printf("\r\n %s ", rbuf.mtext );
	//return (true);
#endif
	ret = msgsnd(msqid, &rbuf, strlen(rbuf.mtext), MSG_NOERROR | IPC_NOWAIT);
	DWORD StartTime = GetTickCount();	
	do
	{
		clr_buffer(rbuf.mtext, MSGSZ);
		rbuf.mtype = 1;
		ret = msgrcv(msqid, &rbuf, 3, 1, MSG_NOERROR | IPC_NOWAIT);
		if (ret>0)
		   printf("rcv: %s\r\n", rbuf.mtext);
		usleep(10);
	} while ((ret <= 0) && (GetTickCount() - StartTime < 2200));
	
	if (ret <=0)
	{
		printf("ERROR \r\n");
		strcpy(msgipc, "ERROR");	
		return (false);
	}
	//printf("konec cteni %s\r\n",rbuf.mtext);
	strcpy(msgipc, rbuf.mtext);
	return (true);
}

bool  Csr::blocked_ipc(const char* stim)
{
	xmsg_buf  rbuf;
	int ret = 0;

	rbuf.mtype = 2;
	clr_buffer(rbuf.mtext, MSGSZ);
	strcpy(rbuf.mtext, stim);
	ret = msgsnd(msqid, &rbuf, strlen(rbuf.mtext), MSG_NOERROR | IPC_NOWAIT);
	DWORD StartTime = GetTickCount();	
	do
	{
		clr_buffer(rbuf.mtext, MSGSZ);
		rbuf.mtype = 1;
		ret = msgrcv(msqrd, &rbuf, 3, 1, MSG_NOERROR | IPC_NOWAIT);
		if (ret > 0)
			printf("rcv: %s\r\n", rbuf.mtext);
	} while ((ret <= 0) && (GetTickCount() - StartTime < 2200));
	
	if (ret <= 0)
	{
		printf("ERROR \r\n");
		strcpy(msgipc, "ERROR");	
		return (false);
	}
	strcpy(msgipc, rbuf.mtext);
	return (true);
}

char *Csr::ipc_msg(void)
{
	return (msgipc);
}


bool Csr::open_adapter(void)
{
	//char adapternumber[20];
	bool ret;
	
	ret = scanhub();
	if (!ret)
	{
		logPrintf("scanhub failed \r\n");
		return (ret);
	}
	
	ret = opendev(500000);
	if (!ret)
	{
		logPrintf("opendev \r\n");
		return (ret);
	}
	clearbuffer(fAdapterNumber, 20);
	ret = getadapternumber(fAdapterNumber);
	
	memcpy(fAdapterUser, &fAdapterNumber[7] , 8);
	memcpy(&fAdapterUser[8], &fAdapterNumber[16], 2); // zkracena varianta
	fAdapterUser[10] = '\0';
	
	if (!ret)
	{
		logPrintf("Cannot read adapter adapter\r\n");
		return (ret);
	}
	return (true);
}

void Csr::data_failure(void)
{
	BOOL ret;
	ret = p3_beep_common();
	usleep(100000);
	
	ret = p3_beep_common();
	usleep(100000);
	
	ret = p3_beep_common();
	usleep(100000);
}

bool Csr::chip_touchx(void) {
#ifdef DEBUG
	logPrintf("chip_touch \r\n");
#endif
	if (is_p3() == false)
	{
		logPrintf("p3 not present \r\n");
		//data_failure();
		return (false);
	}
}

bool Csr::chip_touch(void)
{
#ifdef DEBUG	
	logPrintf("chip_touch \r\n");
#endif
	bool ret = false;
	UCHAR mtype;
	DWORD StartTime = GetTickCount();
	
	ret = medium_type(&mtype);
	while ( (!ret || mtype != 1) && ((GetTickCount() - StartTime) < WAIT_CHIP_TOUCH) )
	{
		usleep(500000);
		ret = medium_type(&mtype);
		if (ret && (mtype != 1))
			ret = p3_reconnect();
	}
	
	
	if (!ret || mtype != 1)
	{
		ret = p3_reconnect();
		if (!ret)
		{
			//logPrintf("p3 not present \r\n");
			//data_failure();
			return (false);
		}
	}	
	return (true);
}

bool Csr::read_and_settime(void)
{
	bool ret ;
	ret = p3_readtime();
	if (!ret)
		p3_settime();
	return (ret);
}

bool Csr::read_p3_header(WORD *firstfree, BYTE *bank)
{
	if (p3_readeeprom() == false)
		return (false);
	
	if (p3header.serial < 100)
		return (false);
	
	*firstfree = 12;
	if (!p3_eventcount(firstfree, bank))
		return (false);
	
	if (*bank > 1)
		return (false);
	
	return(true);
}

void Csr::ProcIni(void)
{
	CSimpleIniA ini;
	const char *tt;

	ini.SetUnicode();
	ini.LoadFile("config.ini");

	//fp3data  = ini.GetValue("OUTPUT", "P3DATA", "/temp/p3.txt");
	//fp3log   = ini.GetValue("OUTPUT", "P3LOG", "/temp/p3log.txt");
	fp3json  = ini.GetValue("OUTPUT", "P3JSON", "/temp/p3.json");
	//     = ini.GetLongValue("OUTPUT", "UDPPORT", 514);
}

int  DoUser(void* data, const char *username)
{
	Csr* csr = static_cast<Csr*>(data);
	logPrintf("%s", username);
	return (true);
}

char *Yhs(WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second)
{
	sprintf(_line, "%2d-%2d-%2d %2d:%2d:%d", year, month, day, hour, minute, second);
	return(_line);
}

long  Ymd(WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second)
{
	time_t timestamp;
	struct tm *ts;
        
    timestamp = time(NULL);
	ts = localtime(&timestamp);
        
	ts->tm_year = year-1900;
	ts->tm_mon = month-1;
	ts->tm_mday  = day;
	ts->tm_hour = hour;
	ts->tm_min  = minute;
	ts->tm_sec  = second;
	
	timestamp = mktime(ts);
	return (timestamp);
}

string convertToString(char *a, int size)
{
	int i;
	string s = "";
	for (i = 0; i < size; i++)
		s = s + a[i];	
	return s;
}


int DoRepx(evrepx evt)
{
	if (_dbopen) 
	{
		char sql[256], s[256], tim[64];
		for (int i = 0; i < 256; i++)
		{
			sql[i] = 0;
			s[i] = 0;
		}	
		Yhs(evt.evs.year, evt.evs.month, evt.evs.day, evt.evs.hour, evt.evs.minute, evt.evs.second);
		strcpy(tim, _line);
		
		strcpy(sql, "INSERT INTO repx (synId, orden, sensor, 'time', chip, type,info, download)");
		strcat(sql, " VALUES ");
		sprintf(s, "( %d, %d, %d, '%s', '%s', %d, %d,CURRENT_TIMESTAMP)", _idSyn, evt.orden, evt.pessn, tim, evt.chip, evt.typ, evt.info);
		strcat(sql, s);
		
		string str = convertToString(sql, sizeof(sql));
		fts.push_back(str);
		
		//do_sql(sql);
		//printf("%s", sql);		
	}
	return (_dbopen);
}

int DoKey(void* data, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE key)
{
	long timestamp;
	char line[30];
	
	logPrintf("%d: %d-%d-%d %d:%d:%d", key, year, month, day, hour, minute, second);
	timestamp = Ymd(year, month, day, hour, minute, second);
	
	sprintf(line, "{\"T\":%ld,\"K\":%d},\r\n", timestamp, key);
	Csr* csr = static_cast<Csr*>(data);
	csr->fjson = csr->fjson + line;
	
	
	_evt.evs.year  = year;
	_evt.evs.month = month;
	_evt.evs.day   = day;
	_evt.evs.hour  = hour;
	_evt.evs.minute = minute;
	_evt.evs.second = second;
	_evt.typ  = 8;
	_evt.info = 0;
	sprintf(_evt.chip, "%d", key);
	_evt.orden++;
	
	DoRepx(_evt);	
	return (true);	
}

int DoAnti(void* data, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE evtype, WORD info)
{
	long timestamp;
	char line[35];
	logPrintf("%d:%d %d-%d-%d %d:%d:%d", evtype,info, year, month, day, hour, minute, second);
	timestamp = Ymd(year, month, day, hour, minute, second);
	
	sprintf(line, "{\"T\":%ld,\"A\":%d,\"F\":%d},\r\n", timestamp, evtype,info);
    Csr* csr = static_cast<Csr*>(data);
	csr->fjson = csr->fjson + line;
		
	_evt.evs.year  = year;
	_evt.evs.month = month;
	_evt.evs.day   = day;
	_evt.evs.hour  = hour;
	_evt.evs.minute = minute;
	_evt.evs.second = second;
	_evt.orden++;
	_evt.typ  = evtype;
	_evt.info = info;
	//strcpy(_evt.chip, ibutton);
	DoRepx(_evt);
	return (true);		
}

void DoError(void* data)
{
	Csr* csr = static_cast<Csr*>(data);
	logPrintf("Error \r\n");
}

// cas predavam v rekordu

int DoSensor(void* data, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, DWORD pessn)
{
	Csr* csr = static_cast<Csr*>(data);
	char line[30];
	
	csr->fjson = "\r\n";
	sprintf(line, "\"version\":%1.1f,\r\n", 1.2);
	csr->fjson = csr->fjson + line;
	
	sprintf(line, "\"adapter\":\"%s\",\r\n", csr->fAdapterNumber);
	csr->fjson = csr->fjson + line;
	
	sprintf(line, "\"pes\":%d,\r\n", pessn);
	csr->fjson = csr->fjson + line;

	sprintf(line, "\"idsyn\":%d,\r\n", _idSyn);
	csr->fjson = csr->fjson + line;

	long timestamp = Ymd(year, month, day, hour, minute, second);
	sprintf(line, "\"timestamp\":%ld,\r\n", timestamp);
	csr->fjson = csr->fjson + line;
	
	sprintf(line, "\"download\":\"%d-%02d-%02d %02d:%02d:%02d\"\r\n", year, month, day, hour, minute, second);
	csr->fjson = csr->fjson + line;
	
	csr->fjson = "{\r\n" + csr->fjson + ","; 
	csr->fjson =  csr->fjson + " \"data\":[ \r\n"; 
	cout << csr->fjson;
	
	// pro zapis do sqlite
	_evt.orden = 0;
	_evt.pessn =  pessn;
	
	logPrintf("%d: %d-%d-%d %d:%d:%d", pessn, year, month, day, hour, minute, second);
	return (true);	
}

int  DoEvent(void* data, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, char *ibutton) 
{	
	char buf[10], line[60];
	long timestamp;
	//sprintf(buf, "%02hhd%02hhd%02hhd", hour, minute, second);
	
	timestamp = Ymd(year, month, day, hour, minute, second);
	sprintf(line, "{\"T\":%ld,\"I\":\"%s\"},\r\n", timestamp, ibutton);
	
	Csr* csr = static_cast<Csr*>(data);
	csr->fjson = csr->fjson + line;
	logPrintf("%s: %4d-%2d-%2d %2d:%2d:%2d", ibutton, year, month, day, hour, minute, second);
	
	
	_evt.evs.year  = year;
	_evt.evs.month = month;
	_evt.evs.day   = day;
	_evt.evs.hour  = hour;
	_evt.evs.minute = minute;
	_evt.evs.second = second;
	_evt.orden++;
	
	strcpy(_evt.chip, ibutton);
    
	DoRepx(_evt);
	
	return (true);
}	


int DoFinal(void* data)
{
	Csr* csr = static_cast<Csr*>(data);
	char line[20];
	std::string key(",");

	// odeber posledni carku a vymen ji za ]
	std::size_t found =  csr->fjson.rfind(key); 
	
	if (found != std::string::npos)
		csr->fjson.replace(found, key.length(), "]");
	
	csr->fjson = csr->fjson + "}";
	printf("\r\n-----------------------------\r\n");
	//printf("%s\r\n", csr->fjson.c_str());
	//logPrintf("Final \r\n");
	
	return (csr->send_json());
}

bool Csr::clr_ipc()
{
	return (false);
}

bool Csr::send_str(const char *ASend)
{
	xmsg_buf rbuf;
	int ret = 0;
	rbuf.mtype = 2;
	clr_buffer(rbuf.mtext, MSGSZ);
	strcpy(rbuf.mtext, ASend);
	ret = msgsnd(msqid, &rbuf, strlen(rbuf.mtext), MSG_NOERROR | IPC_NOWAIT);
	return (ret);
}

bool Csr::wait_for_str(char *Await)
{
	xmsg_buf  rbuf;
	int ret = 0;
	
	DWORD StartTime = GetTickCount();	
	do
	{
		clr_buffer(rbuf.mtext, MSGSZ);
		rbuf.mtype = 1;
		ret = msgrcv(msqrd, &rbuf, 3, 1, MSG_NOERROR | IPC_NOWAIT);
		if (ret > 0)
		{
			printf("rcv: %s\r\n", rbuf.mtext);
			ret = strncmp(rbuf.mtext, Await,strlen(Await)) == 0;
		}
	} while ((ret <= 0) && (GetTickCount() - StartTime < 2200));
	
	return (ret);
}

bool Csr::parse_sst_line(char *cmd)
{

	WORD year;
	BYTE month, day, hour, minute, second;
	time_t current;
	struct tm *timeptr;
	char spom[26];
	BYTE i;
	char sscmd[5], ssdate[10], sstime[10];
		
	// rozbiju radek na tokeny odddelene delimiterem
	char delim[] = " ";
	char *ptr = strtok(cmd, delim);
	strcpy(sscmd, ptr);          // command
	
	if(strcmp(sscmd, "EFB") >= 0)
		return (false);
	
	ptr = strtok(NULL, delim);
	strcpy(ssdate, ptr);		 // date	
	ptr = strtok(NULL, delim);
	strcpy(sstime, ptr);		 // time
	
	printf("%s %s %s\r\n", sscmd, ssdate,  sstime);		
        current = time(NULL);
	timeptr = localtime(&current);
	/*
	sprintf(spom,"GST %04hd-%02hhd-%02hhd %02hhd:%02hhd:%02hhd \r\n",
		timeptr->tm_year+1900,
		timeptr->tm_mon+1,
		timeptr->tm_mday,
		timeptr->tm_hour,
		timeptr->tm_min,
		timeptr->tm_sec); 
        */
	
	char *token = strtok(ssdate, "-");
	sscanf(token, "%d", &timeptr->tm_year);
	timeptr->tm_year = timeptr->tm_year - 1900;
	//printf("timeptr %d\r\n",timeptr->tm_year);

	 // 02
	token = strtok(NULL, "-");
	sscanf(token, "%d", &timeptr->tm_mon);
	timeptr->tm_mon = timeptr->tm_mon - 1;
	//printf(token); printf("\r\n");

	 // 02
	token = strtok(NULL, " ");
	sscanf(token, "%d", &timeptr->tm_mday);
	//printf(token); printf("\r\n");

	 // 14
	token = strtok(sstime, ":");
	sscanf(token, "%d", &timeptr->tm_hour);
	//timeptr->tm_hour=10;
	//printf(token); printf("\r\n");

	 //30
	token = strtok(NULL, ":");
	sscanf(token, "%d", &timeptr->tm_min);
	//printf(token); printf("\r\n");

	 // 25
	token = strtok(NULL, "\r\n");
	sscanf(token, "%d", &timeptr->tm_sec);
	//printf(token); printf ("\r\n");

	current = mktime(timeptr);
	//printf("Datetime before reset %s\n", ctime(&current));
	
	const struct timeval tv = { mktime(timeptr), 0 };
	settimeofday(&tv, 0);
	//stime(&current);
	printf("Datetime after reset %s\n", ctime(&current));
	return (TRUE);
}

void Csr::setDate(const char* dataStr)  // format like MMDDYY
{
	char buf[3] = { 0 };

	strncpy(buf, dataStr + 0, 2);
	unsigned short month = atoi(buf);

	strncpy(buf, dataStr + 2, 2);
	unsigned short day = atoi(buf);

	strncpy(buf, dataStr + 4, 2);
	unsigned short year = atoi(buf);

	time_t mytime = time(0);
	struct tm* tm_ptr = localtime(&mytime);

	if (tm_ptr)
	{
		tm_ptr->tm_mon  = month - 1;
		tm_ptr->tm_mday = day;
		tm_ptr->tm_year = year + (2000 - 1900);

		const struct timeval tv = { mktime(tm_ptr), 0 };
		settimeofday(&tv, 0);
	}
}


bool Csr::wait_for_line(const char *stim, char *Await)
{
	xmsg_buf  rbuf;
	int ret = 0;
	
	// rekni si o explictini cas ze serveru
	send_str(stim);
	
	DWORD StartTime = GetTickCount();	
	do
	{
		clr_buffer(rbuf.mtext, MSGSZ);
		rbuf.mtype = 1;
		ret = msgrcv(msqrd, &rbuf, MSGSZ, 1, MSG_NOERROR | IPC_NOWAIT);			
	} while ((ret <= 0) && (GetTickCount() - StartTime < 2200));
	
	strcpy(Await, rbuf.mtext);
	
	
	return (ret>0);
}	

bool Csr::test_jsonwrite(char *filename)
{
	std ::string line;
	bool ret;
	std::ifstream file(filename);
	char lin[60];
	
    if (!file.is_open())
		return (false);

	// SJS ... start json prenosu, python se prepne do rezimu prenosu
	if (blocked_ipc("SJS") == false)
	{
		logPrintf("Neni odpoved SJS \r\n");
		return (false);
	}
	
	// pretahni jednotlive radky
	printf("--------------\r\n");
	while (getline(file,line)) 
	{
		if (blocked_ipc(line.c_str()) == false)
		 return (false);
	}
	file.close();

	// konec json prenosu, pockej si na potvrzeni z pythonu
	printf("*************\r\n");
	
	//char q[] = "EJS";
	send_str((char *)"EJS");
	ret = wait_for_str((char *)"EJK");

	return (true);
}


bool Csr::send_json(void)
{
	/*
	if (blocked_ipc("SJS") == false)
	{
		logPrintf("Neni odpoved SJS \r\n");
		return (false);
	}
	*/
	
	int size = strlen(fjson.c_str());
	int minalo = size / 64 + 1;
	
	char *str; 
	str= (char *)malloc(minalo * 64);
	memset(str, 0xFF, minalo * 64);	
	if (str == 0)
	{
		printf("ERROR: Out of memory\n");
		return 1;
	}
	strcpy(str, fjson.c_str());
	printf("***************** size=%d\r\n",size);
	
	// budu posilat JSON
	if (blocked_ipc("SJS") == false)
	{
		logPrintf("Neni odpoved SJS \r\n");
		return (false);
	}
		
	char delim[] = "\r\n";
	char *ptr = strtok(str, delim);
	while (ptr != NULL)
	{
		// posilam jednotlive radky
		printf("'%s'\n", ptr);
		if (blocked_ipc(ptr) == false)
			return (false);
		
		ptr = strtok(NULL, delim);	
	}
	free(str);
	
	send_str("EJS");
	bool ret = wait_for_str((char *)"EJK");	
	return (ret);
	
}

bool Csr::read_p3(WORD *firstfree)
{
	int minalo = (*firstfree / 64 + 1);
	pcBufRead = (BYTE *) malloc(minalo * 64);
	memset(pcBufRead, 0xFF, minalo * 64);
	
	if (!p3_readtourwork(*firstfree, pcBufRead))
		return (false);
	
	// firstfree >0 ...do i have events in stream  ?	
	// save the stream into database 
	return (true);	
}

int Csr::save_blob(WORD *firstfree)
{
	_idSyn = -1;
	if (_dbopen > 0)
	{
		// this is new download
		_idSyn = new_file(*firstfree);  //idSyn ziskam z tablky gensyn
		write_blob(pcBufRead,*firstfree, _idSyn); // blob,velikost,a cislo synchra
		_evt.idSyn = _idSyn;
		for (int i = 0; i < *firstfree; i++)
			printf("%2x ", pcBufRead[i]);
		printf("\r\n");
	}
	return (_idSyn);
}

bool Csr::parse_data(WORD *firstfree,int _idSyn)
{
	fpc._idSyn            = _idSyn; // lokalni cislo synchronizace
	
	fpc.procData          = this;
	fpc.timeCheck         = NULL;
	fpc.connectionCheck   = NULL;
	fpc.error             = DoError;
	fpc.processUsername   = DoUser;
	fpc.processTouch      = DoEvent;
	fpc.processSensor     = DoSensor;
	fpc.processKey        = DoKey;
	fpc.processAntivandal = DoAnti;
	fpc.finalize          = DoFinal;
	return(parsedump(pcBufRead, *firstfree, &fpc));
}


int Csr::mLoop()
{
	bool ret = false;
	WORD firstfree;
	BYTE bank;
	int  iSat = 0;

	char cmd[21];
		
	_blinkFork   = 0;
	_blinkState  = 0;
	fMachineState = m_start;
	char line[90];
	static int iWaitCnt = 0;
	static bool fDBOK = FALSE; 
	static int StartTime = 0;
	const char *lan_reader_cfg = "../lan_reader_cfg.txt";
	
	do
	{
		switch (fMachineState)
		{
		case m_start:
#ifdef DEBUG 
			logPrintf("m_start \r\n");
#endif		
			//parse_ini_file(lan_reader_cfg);
					
			ret = open_adapter();
			if (ret) {
				logPrintf("Cislo adapteru:%s\r\n", fAdapterNumber);
				fMachineState = m_sendadapter;
			}
			parse_ini_file("lan_reader_cfg.txt");
			
			//char srv[] = "10.0.0.115";
			//int  port  = 5000;
			//char cmd[] = "CHK\r\n";
			//send_cmd(srv, port, cmd);
			//send_cmd("10.0.0.115", 5000, "CHK\r\n");

			StartTime = GetTickCount();  // milisekund od startu operacniho systemu

			//const char *p = "./test.db";
			if (open_database("./test.db"))
			{
				// podivej se, jestli mame nepreneseny blob 
				_dbopen = 1;
				int tok;

				_evt.idSyn = untransfered_id("rawbuf",&tok,&firstfree);	
				if ((tok <1) && (_evt.idSyn>0))
				{
					char sql[256];
					//sprintf(sql, "update rawbuf set tok=1 where idSyn=%d;\r\n", _evt.idSyn);
					//do_sql(sql);
					fMachineState = m_waitforadapter;

					/*
					int minalo = (firstfree / 64 + 64);
					pcBufRead = (BYTE *) malloc(minalo * 64);
					memset(pcBufRead, 0xFF, minalo * 64);
					int blobsize = 0;
					read_blob(_evt.idSyn, &pcBufRead,&blobsize,&minalo);

					fMachineState = m_parsedata;
					*/
					break;
				}
			}	
			msleep(2000);
			
			fMachineState = m_waitforadapter;
			break;
		
		case m_sendadapter:
#ifdef DEBUG
			logPrintf("m_sendadapter \r\n");
#endif

			sprintf(cmd, "ADA %s", fAdapterUser);  // ADA ... send compressed adapter number to python
			ret = wait_for_line(cmd, line);
			if (ret > 0) {
				logPrintf("ADA->%s\r\n", line);
				fMachineState = m_checkserver;
			}
			fMachineState = m_start;
			break;
	  
		case m_checkserver:
#ifdef DEBUG
			logPrintf("m_checkserver \r\n");
#endif
			fDBOK = false;		
			//char cmd[21];
			//sprintf(cmd, "CFF %s", fAdapterUser);  // CFF ... check for firebase/server
			ret = wait_for_line("CFF", line);
			if (ret>0)
			{
				logPrintf("CFF->%s\r\n",line);
				if (strstr(line, "EFB") == NULL) // EFB ... error firebase/server (timeout)
				{
					//parse_sst_line(line);			
					stop_blinking();
					fMachineState = m_serverok;	
					iSat = 0;
				
					/*
					int diff = (GetTickCount() - StartTime) / 1000;  // rozdil vedu v sekundach
					if (diff > RMD_SECONDS ) {
						fMachineState = m_checkmissing;
						StartTime = GetTickCount();
					}
					break;
					*/

					fMachineState = m_checkmissing;

				}
			}
			;
			
			if (iSat++ > SATCNT)
				fMachineState = m_serverError;		
			break;

		case m_checkmissing:
#ifdef DEBUG
			logPrintf("m_checkserver \r\n");
#endif			
			// Do i need to remedy missing data ?
			_isResend = false;

			sprintf(cmd, "RMD %s", fAdapterUser);  // ADA ... send compressed adapter number to python
			ret = wait_for_line(cmd, line);
			if (ret > 0)
			{
				// co se mi vratilo jako odpoved na RMD %s(cislo adapteru)
				logPrintf("RMD->%s\r\n", line);
				if (strstr(line, "RMD") != NULL)
				{
					// OK, there is request to re-send missing data
					//int synId = -1; // cislo synchronizace, ktere chci odeslat
					_idSyn = -1;
					int iinto = -1;
					remedy(line, &_idSyn, &iinto); // ffree je na pozici into,
					//*pcBufRead = NULL;

					 //_idSyn = 322;  // vynutim cislo synchronizace, ktere chci odeslat

					// kdyz se mi na RMD vrati 0, tak nemam co posilat
					if (_idSyn<1) {
						fMachineState = m_serverok;
						break;
					}

					// dohraj si parametry z databaze a zavolej parser
					int ffree  = -1;
					int blobsize =0;
					p3header.serial = get_sensorid(_idSyn);
					read_blob(_idSyn ,&pcBufRead,&blobsize,&ffree);  // v into bude pocet bytu blobu
					firstfree = ffree;
					dumpBuffer(pcBufRead, ffree);

					// handle missing data ...

					_isResend = true;
					StartTime = GetTickCount();
				   	//_idSyn = -1 * _idSyn;
					fMachineState = m_parsedata;
					iSat = 0;
					break;
				}
				if (iSat++ > SATCNT)
					fMachineState = m_serverError;		
				break;
			}
	
		case m_serverError:
#ifdef DEBUG
			logPrintf("m_serverError \r\n");
#endif			
			if (_blinkState ==0)
		  	  start_blinking();
			fMachineState = m_waitforserver;
			msleep(1000);
			break;
		
		case m_waitforserver:
#ifdef DEBUG
			logPrintf("m_waitforserver \r\n");
#endif
			usleep(5000000);
			fMachineState = m_checkserver;
			break;
				
		case m_serverok:
#ifdef DEBUG
			logPrintf("m_serverok \r\n");
#endif
			stop_blinking();
			fMachineState = m_waitforadapter;
			break;	
		
		case m_waitforadapter:
#ifdef DEBUG
			logPrintf("m_waitforadapter \r\n");
#endif
			clearbuffer(fAdapterNumber, 20);
			if (getadapternumber(fAdapterNumber) == true)
			{
				logPrintf("AdapterNumber: %s AdapterUser=%s\r\n", fAdapterNumber,fAdapterUser);
				fMachineState = m_waitforpes;
				break;
			}	
			_poweroff();
			//fMachineState = m_checkserver;
			fMachineState = m_sendadapter;
			break;
	
		case m_waitforpes:
#ifdef DEBUG
			logPrintf("m_waitforpes %d \r\n",iWaitCnt);
#endif
			fMachineState = m_checkserver;
			//if (chip_touchx())
			if (is_p3())
			{
				fMachineState = m_readp3header;
				break;
			}	
			usleep(990000);
                         
			// get time from server
            if (iWaitCnt>=SYNCHROTIMEWAIT)
			{
			  fMachineState = m_servertime;
			  iWaitCnt=0;
			}
			iWaitCnt++;
			break;
			
		case m_servertime:
#ifdef DEBUG
			logPrintf("m_servertime \r\n");	
#endif
			if (wait_for_line("CFF",line) == TRUE)
			{
				printf("line: %s", line);
				parse_sst_line(line);
				fMachineState = m_readsettime;
				break;
			}
			msleep(5000);
			break;
			
			
		case m_readsettime:
#ifdef DEBUG
			logPrintf("m_readsettime \r\n");
#endif
			if (read_and_settime())
				fMachineState = m_readp3header;
			else 
				fMachineState = m_checkserver;
			break;
	
		case m_readp3header:
#ifdef DEBUG
			logPrintf("m_readp3header \r\n");
#endif
			if (read_p3_header(&firstfree, &bank))
			{
				if (firstfree > 0)
				{
					fMachineState = m_readp3;
					break;
				}
				logPrintf("p3 is empty \r\n");
			}
			usleep(900000);
			fMachineState = m_waitforadapter;
			break;
			

		case m_quick_srv_check:	
#ifdef DEBUG
			logPrintf("m_quicksrv_check\r\n");	
#endif
			if (conn_test(_host, _port, 2) !=0);			
			{
				// neni spojeni na server, zacni bliket
				fMachineState = m_serverError;
				break;
			}
			fMachineState = m_readp3;
			break;
			
		case m_readp3:
#ifdef DEBUG
                        //firstfree=2612;
			logPrintf("m_readp3, firstree=%d \r\n",firstfree);
#endif
			if (read_p3(&firstfree))
			{
				//fMachineState = m_checkserver;
				//usleep(1000000);
				fMachineState = m_prepdelp3;
				iSat = 0;
				break;
			}	
			fMachineState = m_waitforadapter;
		    break;
			
		case m_prepdelp3:
#ifdef DEBUG
			logPrintf("m_prepdelp3 \r\n");
#endif
			if (chip_touch() == true)
			{        
				if (p3_reconnect())
				{
					fMachineState = m_deletep3;
					iSat = 0;
					break;
				}
			     
				if (iSat++ > DELETE_ATT)
					fMachineState = m_checkserver;     
				break;
			}
			else
			{
				if (iSat++ > DELETE_ATT)
					fMachineState = m_checkserver;
				
				// chip_touch doesnt work, expecting p3 on site
				clearbuffer(fAdapterNumber, 20);
				if (getadapternumber(fAdapterNumber) == true)
				{
					logPrintf("m_prepdelp3 restart adapter: %s (att:%d) \r\n", fAdapterNumber, iSat);
					//fMachineState = m_waitforpes;
					break;
				}	
				_poweroff();		
			}		
			break;
		
		case m_deletep3:
#ifdef DEBUG
			logPrintf("m_deletep3 \r\n");	
#endif
			if (p3_delete())
			{
#ifdef DEBUG
				logPrintf("...p3 deleted \r\n");	
#endif
				fMachineState = m_settimep3;
				break;
			}
			
			if (!p3_reconnect())
				logPrintf("trying to reconnect \r\n");
			
			if (iSat++ > DELETE_ATT)
				fMachineState = m_checkserver;
			
			//usleep(500000);
			break;

		case m_settimep3:
#ifdef DEBUG
			logPrintf("m_settime \r\n");
#endif			
			if (p3_settime())
			{
				fMachineState = m_beepok;
				break;
			}
			
			if (!p3_reconnect())
				logPrintf("trying to reconnect \r\n");			
			usleep(100);
			break;
			

		case m_beepok:
#ifdef DEBUG
			logPrintf("m_beepok \r\n");
#endif
			p3_beep_ok() ;
			fMachineState = m_saveblob;
			break;
			
		case m_saveblob:
#ifdef DEBUG
			logPrintf("m_saveblob\r\n");
#endif
			fts.clear();
			_idSyn = save_blob(&firstfree);
			fMachineState = m_parsedata;
			iSat = 0;
			break;
			
		case m_parsedata:
#ifdef DEBUG
			logPrintf("m_parsedata \r\n");
			//dumpBuffer(pcBufRead,firstfree);
#endif
			if (parse_data(&firstfree,_idSyn))
			{
				/*
				close_database();
				open_database("./test.db");
				*/
				// do blobu zapis, ze jsem odeslal data
				char sql[256];
				sprintf(sql, "update rawbuf set tok=1 where idSyn=%d;\r\n", _evt.idSyn);
				do_sql(sql);

				// tady ukladam sql k sobe do databaze
				// nechci ukladat sql, ktere odesilam jako remedy na server
				// a ktere uz v litesql existuji
				if (_isResend == false)
				{
					// vypis, co mam v tstringListu
					std::vector<std::string>::iterator iter = fts.begin();
					std::vector<std::string>::iterator end  = fts.end();

					logPrintf("m_saving_sql\r\n");
					while (iter != end)
					{
						std::cout << *iter << "\r\n";
						string s = *iter;
						strcpy(sql, s.c_str());
						do_sql(sql);
						++iter;
					}
					logPrintf("\r\n");
				}

				iSat = 0;
				fMachineState = m_checkserver;
				break;
			}
						
			if (iSat++ > DELETE_ATT)
				fMachineState = m_checkserver;
						
			break;
						
		case m_finishp3:
			logPrintf("FINISH \r\n");
			break;
		}
		//printf("....\r\n");
	} while (fMachineState != m_finishp3);
	return (true);
}

Csr::Csr()
{
	create_pipe();
}

void die(char *s)
{
	close_database();
	perror(s);
	exit(1);
}

 void* thread_start(void *arg)
{
	char adapternumber[20];
	BOOL ret = 0;

	logPrintf("blink start\n");
	while (_blinkState != 0)
	{
		clearbuffer(adapternumber, 20);
		ret = getadapternumber(adapternumber);
		usleep(blink_interval_us);
		_poweroff();
		usleep(blink_interval_us);
	}
	logPrintf("thread-exits\n");
	return NULL;
}

void Csr::stop_blinking(void)
{
	void *thret;
	if (_blinkFork != 0)
	{
		_blinkState = 0;
		pthread_join(_blinkFork, &thret);
		_blinkFork = 0;
	}
}

void Csr::start_blinking(void)
{
	pthread_t tpid;
	pthread_attr_t attr;
	int ret;
	
	ret = pthread_attr_init(&attr);
	if (ret == 0)
	{
		_blinkState = 1;
		ret = pthread_create(&tpid, &attr, &thread_start, NULL);
		if (ret == 0)
		{
			_blinkFork = tpid;
		}
		else
		{
			logPrintf("pthread_create-blink failed\n");
			_blinkState = 0;
		}
		pthread_attr_destroy(&attr);
	}
	
}


