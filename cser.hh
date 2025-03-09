// System libraries like timeouts etc.
#ifndef CSER_H
#define CSER_H
#include <sys/msg.h>

#define  LOWBYTE(v)   ((unsigned char) (v))
#define  HIGHBYTE(v)  ((unsigned char) (((unsigned int) (v)) >> 8))
#define  MAXMSGSIZE 20
#define  JSONSIZE 4096
#define  MSGSZ  128

//#include "asmjs.hh"
#include <string>
#include <fstream>
using namespace std;

extern "C" {
	//#include "WinTypes.h"
	//#include "engine.h"
	#include "parser.h"
	#include "comutils.h"
}


typedef enum 
{
	m_start,
	m_sendadapter,
	m_checkserver,
	m_checkmissing,
	m_serverError,
	m_waitforserver,
	m_serverok,
	m_waitforadapter,
	m_waitforpes,
	m_servertime,
	m_readsettime,
	m_settimep3,
	m_readp3header,
	m_quick_srv_check,
	m_readp3,
	m_saveblob,
	m_prepdelp3,
	m_deletep3,
	m_beepok,
	m_parsedata,
	b_beepfailure,
	m_finishp3
} machine_state;


class Csr
{
private: 
	char fAdapterNumber[20];
	
	char msgipc[MAXMSGSIZE];
	//char fjson[JSONSIZE];
	struct msgbuf fsbuf;

	typedef struct msgbuf {
		long    mtype;
		char    mtext[MSGSZ];
	} xmsg_buf;
	
	parseCallbacks fpc;
	machine_state fMachineState;
	UCHAR *pcBufRead;

	std::string fp3data;  // folder to output data
	std::string fp3log;   // folder for log output
	std::string fp3json;  // folder to json output

	std::fstream fsp3json;   // json output
	int fudp;		      // folder to json output
	void ProcIni(void);
	
	bool chip_touch(void);
	bool chip_touchx(void);
	void stop_blinking (void);
	void start_blinking(void);
	void data_failure(void);
	
	
	friend void* thread_start(void *);
	friend int  DoUser  (void*, const char *username);
	friend int  DoSensor(void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, DWORD pessn) ;
	friend int  DoEvent (void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, char *ibutton); 
	friend int  DoKey   (void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE key);
	friend int  DoAnti  (void*, WORD year, BYTE month, BYTE day, BYTE hour, BYTE minute, BYTE second, BYTE evtype, WORD info);
	friend void DoError (void*);
	friend int  DoFinal (void*);
	
	void clr_buffer(char* val, char size);
	bool open_adapter(void);
	bool create_pipe(void);
	bool put_msg(const char *msg);
	int  get_msg(char *msoutput);
	//bool wrt_read(const char* ainput, char *aoutput);
	bool read_and_settime(void);
	bool read_p3_header(WORD *firstfree, BYTE *bank);
	bool read_p3(WORD *firsfree);
	int save_blob (WORD *firstfree);
	bool parse_data(WORD *firstfree, int idSyn);
	
public:
	Csr();
	~Csr();

	string fjson;
	bool test_msg(const char* stim);
	bool wrt_read(const char* ainput, char* aoutput);
	char* ipc_msg(void);
	int mLoop(void);
	void setMachine(machine_state Astate);
	bool test_jsonwrite(char* filename); // odesle pripraveny json file do pythonu
	bool blocked_ipc(const char* stim);
	bool clr_ipc();
	bool wait_for_str(char* Await);
	bool send_str(const char* ASend);
	bool send_json(void);
	bool wait_for_line(const char *stim,char *Await);
	bool parse_sst_line(char *cmd);
	void setDate(const char* dataStr);  // format like MMDDYY
	
};

#endif
