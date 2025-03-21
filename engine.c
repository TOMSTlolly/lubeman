//#define FTD2XX
//#define DEBUG 0
//#define DEBUG_FTDI 0
//#define TDEB 1

#include "ftd2xx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "comutils.h" 
#include "WinTypes.h"
#include "parser.h"
#include "engine.h"
#include "unistd.h"

#define BUF_SIZE 100
#define MAX_DEVICES 5

FT_STATUS       ftStatus;
FT_HANDLE       ftHandle[MAX_DEVICES];

// temporary variables used by scanhub routines
// they are then used by opendev() routine
char *  pcBufLD[MAX_DEVICES + 1];
char    cBufLD[MAX_DEVICES][64];

// Essential variables used by engine
char   cBufRead[BUF_SIZE];       // buffer prepared for respond 
char    cmd[BUF_SIZE];            // command line in hexa format, parameters are separated by ;
int     iNumDevs = 0;
int     iDevicesOpen;
int     iDeviceIndex;

// internal eeprom storage contains antivandal poinst, sensor number, etc. see engine.h
PesEepromRecord p3header;


// scan hub for TMD device
BOOL scanhub(void)
{
 int i;

     // initialize pointers 
     for(i = 0; i < MAX_DEVICES; i++) {
         pcBufLD[i] = cBufLD[i];
     }
     pcBufLD[MAX_DEVICES] = NULL;

     ftStatus = FT_ListDevices(pcBufLD, &iNumDevs, FT_LIST_ALL | FT_OPEN_BY_SERIAL_NUMBER);
     if(ftStatus != FT_OK) {
       printf("Error: FT_ListDevices(%d)\n", (int)ftStatus);
       return 1;
     }
	
    iDeviceIndex = -1;
    for (i = 0; ((i < MAX_DEVICES) && (i < iNumDevs)); i++) {
	    printf("Device %d Serial Number - %s\r\n", i, cBufLD[i]);
	    
	    FT_Close(&ftHandle[i]);
  
	    if ((ftStatus = FT_OpenEx(cBufLD[i], FT_OPEN_BY_SERIAL_NUMBER, &ftHandle[i])) != FT_OK)
	    {
		    printf("Error FT_OpenEx(%d), device %d\n", (int)ftStatus, i);
		    exit(0);
		    return FALSE;
	    }
	    iDeviceIndex = i;
	    printf("Opened device %d,serial %s\r\n", iDeviceIndex, cBufLD[i]); 
	    return (TRUE);
    }
 }
	
// trying to open device using TMD descriptor
// or using serial number
BOOL opendev(unsigned int baudrate)
{
	int i;
	if ((ftStatus = FT_SetBaudRate(ftHandle[iDeviceIndex], 500000)) != FT_OK)
	{
		printf("Error FT_SetBaudRate(%d), cBufLD[i] = %s\n", (int)ftStatus, cBufLD[i]);
		return (FALSE);
	}
	return (TRUE);
}


BOOL closedev(void)
{
 FT_Close(cBufLD[iDeviceIndex]);
 printf("Closed device %s\n", cBufLD[iDeviceIndex]);

}

FT_STATUS _poweroff(void)
{
FT_STATUS ret;
   if ((ret=FT_ClrRts(ftHandle[iDeviceIndex]))!=FT_OK)
     return(ret);
 
    if ((ret=FT_ClrDtr(ftHandle[iDeviceIndex]))!=FT_OK)
      return(ret);
}


// power the adapter ON
FT_STATUS  _poweron(void)
{
  FT_STATUS ret;
  if ((ret=FT_ClrRts(ftHandle[iDeviceIndex]))!=FT_OK)
    return(ret);
  
  if ((ret=FT_ClrDtr(ftHandle[iDeviceIndex]))!=FT_OK)
    return(ret);  

  msleep(10);
  if ((ret=FT_SetRts(ftHandle[iDeviceIndex])) !=FT_OK)
    return(ret);
  
  if ((ret=FT_SetDtr(ftHandle[iDeviceIndex])) !=FT_OK)
    return(ret);

  msleep(1);
  
  return(FT_OK);
}

// calculate check for buffer we are going to send to TMD
BYTE input_check(unsigned char *buf)
{
  BYTE i,cnt,d;
  d = 0;
  cnt = buf[0];
  for (i=2;i<cnt;i++)
    d = d + buf[i];

  d = d % 256;
  d = 0x100 - d;
  d = d % 256;
  buf[cnt]=d;
  return (d);
}

// convert *cmd -> cBufWrite
WORD prepare_buffer(char *cmd, UCHAR *cBufWrite)
{
  static WORD i,j;
  j=0;
 // printf("%s\n",cmd);

  // *********  tokenize *cmd into fbuf
  char *token = strtok(cmd,";");
  while (token !=NULL)
  {
    if (token[0]=='$')
      sscanf(token+1,"%x",&i);
    else
      sscanf(token,"%d",&i);
    token=strtok(NULL,";");
    cBufWrite[j]=i;

#ifdef DEBUG_FTDI
    printf("%02X, ",(unsigned int)cBufWrite[j]);
#endif
    j++;
  }


  return (j);
}

// writes command to the FTDI device 
// check  ... calculate check and append it to the end
BOOL writebuf(char *cmd, BOOL check)
{
  WORD j;
  DWORD dwBytesWritten;
  unsigned char cBufWrite[BUF_SIZE]; 
  BYTE chk;

  // returns tokenized cmd. J is number of tokens
  j=prepare_buffer(cmd,cBufWrite);
  if (check==TRUE)
  {
    j++;
    #ifdef DEBUG_FTDI
       printf("before check device %d, bytes %d\n",iDeviceIndex,j);
       dumpBuffer(cBufWrite,j);
    #endif
    usleep(10);
    chk=input_check(cBufWrite);
  }
  // ********  write into device
  #ifdef DEBUG_FTDI
       printf("Write into device %d, bytes %d chck=%2X \n",iDeviceIndex,j,chk);
       dumpBuffer(cBufWrite,j);
  #endif

  ftStatus=FT_Write(ftHandle[iDeviceIndex],cBufWrite,j,&dwBytesWritten);
  if (ftStatus != FT_OK)
  {
     #ifdef DEBUG_FTDI
     printf("[Erorr(%d), iDevIndex(%d)] Cannot write into adapter\n",(int)ftStatus,iDeviceIndex);
     #endif
     return (FALSE);
  }
  if (dwBytesWritten != j)
  {
     #ifdef DEBUG_FTDI
     printf("FT_Write only wrote %d (of %d) bytes\n",(int)dwBytesWritten,BUF_SIZE);
     #endif
     return(FALSE);
  }
  return (TRUE);
}


// wait for cnt bytes in queue
// please note the data are still in queue
BOOL wait_for_queue(BYTE cnt)
{
  DWORD dwRxSize;
  unsigned int StartTime; 

  dwRxSize = 0;
  StartTime = GetTickCount();
  while ((dwRxSize < cnt) && ((GetTickCount()-StartTime)<600))  {
    ftStatus = FT_GetQueueStatus(ftHandle[iDeviceIndex], &dwRxSize);
  }
  return ((ftStatus==FT_OK)) ;
}

BOOL readbuf(BYTE cnt, UCHAR *pcBufRead)
{
  DWORD dwBytesRead,dwRxSize;

  // *******  wait for respond
  if (wait_for_queue(cnt)==TRUE)
  {
     dwRxSize = cnt;  // We know exact number of bytes comming from adapter
     
     #ifdef FTDI_DEBUG
       printf("Calling FT_Read awaiting (%d) bytes :\n",dwRxSize);
     #endif
     ftStatus = FT_Read(ftHandle[iDeviceIndex], pcBufRead, dwRxSize, &dwBytesRead);
     if (ftStatus != FT_OK) {
       #ifdef FTDI_DEBUG
	 printf("Error FT_Read(%d)\n", (int)ftStatus);
       #endif
       return(FALSE);
     }
     if (dwBytesRead != dwRxSize) {
	#ifdef FTDI_DEBUG
	  printf("FT_Read only read %d (of %d) bytes\n",
	      (int)dwBytesRead,
	      (int)dwRxSize);
	#endif
	return(FALSE);
     }

     #ifdef FTDI_DEBUG
       printf("FT_Read read %d bytes. Read-buffer is now:\n", (int)dwBytesRead);
       dumpBuffer(pcBufRead, (int)dwBytesRead);
     #endif

     return(TRUE);
  } // if (ftStatus == FT_OK)
  #ifdef FTDI_DEBUG
  else
    printf("wait_for_queue not responding \n");
  #endif 
}

BOOL cmdsim(char *cmd, BYTE cnt,BOOL check, UCHAR *pcBufRead)
{
  // *******  writes command to the device
  if (writebuf(cmd,check)==FALSE)
     return(FALSE);
 
  // ******  wait for the respond
  if (readbuf(cnt,pcBufRead) ==FALSE)
     return(FALSE);

  return (TRUE);
}


// put command into adapter and wait for expected number of  bytes back
// cmd ... general command 
// cnt ... expected number of bytes returning as answer from TMD adapter
// check...we are calculate check at the end of the buffer
// pcBufRead... returning buffer with answer from TMD 
BOOL cmdapi(char *cmd, BYTE cnt, BOOL check, UCHAR *pcBufRead)
{
   WORD i,j;
   DWORD   dwRxSize = 0;
   DWORD   dwBytesRead;
 
   // *******  writes command to the device
   if (writebuf(cmd,check)==FALSE)
     return(FALSE); 
 
   // wait for first byte in queue 
   if (wait_for_queue(1)==TRUE) 
     if (readbuf(1,pcBufRead)==TRUE)
       if (pcBufRead[0]>0)
       {
		 printf("timeout api (%d)\n",pcBufRead[0]);
		 return(FALSE);
       }
    
   // ******  wait for the respond  
   if (readbuf(cnt-1,&pcBufRead[1]) ==FALSE)
   {
     return(FALSE); 
   }
   return (TRUE);
}

void fmt_adaptornumber(char *buf_input,char *AdapterNumber)
{
  int i;
  char hex[2];
  ChipIDDef fid;  // array of 8 bytes

  // permut fields and prepare them for formatting
  fid[0]=0;
  fid[1]=0;
  fid[2]=0;
  fid[3]=buf_input[1]; 
  fid[4]=buf_input[2];
  fid[5]=buf_input[3];
  fid[6]=buf_input[4];
  fid[7]=254;
  
  // compute crc
  fid[7]=calc8(fid);
  clearbuffer(AdapterNumber,18); 
 
  // prefix 
  sprintf(hex,"%02X",fid[0]);
  strcat(AdapterNumber,hex);
  strcat(AdapterNumber,"-");
  
  // meat
  for (i=1;i<=6;i++) {
   sprintf(hex,"%02X",fid[i]); 
   strcat(AdapterNumber,hex);
  }  
 
  // crc
  strcat(AdapterNumber,"*");
  sprintf(hex,"%02X",fid[7]);
  strcat(AdapterNumber,hex);
  
}

// trying to start adapter
// if suceeded, routine returns its serial number
BOOL getadapternumber(char* AdapterNumber)
{
 BOOLEAN ret;

 _poweroff();
  usleep(2000);
	
	
 if ( _poweron()!=FT_OK)
 {
   printf ("_poweron() failed\n"); 
   return(FALSE);
 }

 clearbuffer(cBufRead,BUF_SIZE); 
 // send order to get adapter number
 strcpy(cmd,"1;$F0");
 ret=cmdapi(cmd,5,FALSE,cBufRead); 
 //dumpBuffer(cBufRead,5);
 
 if (!ret)
   return (FALSE);

 fmt_adaptornumber(cBufRead,AdapterNumber);
  
 return(TRUE);
}

BOOL tmd_version(UCHAR *hw,UCHAR *fw)
{
  clearbuffer(cBufRead,BUF_SIZE); 
  strcpy(cmd,"1;$F1");
  if (cmdapi(cmd,3,FALSE,cBufRead)==FALSE) // sending order, waiting 3 bytes in cBufRead 
  {
    printf("Cannot get tmd version, command failed\n");
    return (FALSE);
  }
  if (cBufRead[0]==0)
  {
    hw[0]=cBufRead[1];
    fw[0]=cBufRead[2];
    return (TRUE);
  }
 
  return (FALSE);
}

BOOL is_p3(void) {
  strcpy(cmd,"1;$C0");
  BOOL b = cmdsim(cmd, 1, FALSE, cBufRead);
  if (b == FALSE)
    return FALSE;

  if (cBufRead[0]==0)
    return FALSE;

  // tohle by melo vratit 3 pro P3
  strcpy(cmd,"1;$D0");
  b = cmdsim(cmd,1,FALSE,cBufRead);
  if (b == FALSE)
    return FALSE;

  if (cBufRead[0]==3)
    return TRUE;

}


BOOL medium_type(UCHAR *medium)
	{
	  strcpy(cmd,"1;$C0");
	  BOOL b = cmdsim(cmd, 1, FALSE, cBufRead);
#ifdef DEBUG
		logPrintf("medium_type %d \r\n",(BYTE)b);
		dumpBuffer(cBufRead, 4);
#endif
      if (b == FALSE)		
	  //if (cmdsim(cmd,1,FALSE,cBufRead)==FALSE)
	  {
#ifdef DEBUG
	    logPrintf("Cannot get media type, command failed\n");
#endif
	    return(FALSE);
	  }
	   
	  *medium = cBufRead[0];
	  return (TRUE);
	 // #ifdef DEBUG
	 // printf("medium signature %d\n",cBufRead[0]);
 // #endif
}

BOOL p3_reconnect(void)
{
  // type of media on TMD probe
  strcpy(cmd,"1;$C0");
#ifdef DEBUG_FTDI
     logPrintf("p3_reconnect() routine \r\n") ;
#endif 
  // FALSE in cmdapi, do not calculate check for input buffer
  int b = cmdsim(cmd, 1, FALSE, cBufRead);
#ifdef DEBUG_FTDI
//	dumpBuffer(cBufRead, 4);  
#endif
  if (b==FALSE)
  {
#ifdef DEBUG_FTDI
      logPrintf("Cannot recognize media type on probe\n");  
#endif
   
    return(FALSE);
  }
  
  // is sensor present on TMD ?
  if (cBufRead[0]==1)
  {
#ifdef DEBUG_FTDI
    printf("medium signature %d\n",cBufRead[0]);
#endif

    strcpy(cmd,"1;$D0");
    if (cmdsim(cmd,1,FALSE,cBufRead)==FALSE)
    {
#ifdef DEBUG
      logPrintf("\r\rCannot recoginze type of the sensor (P2/P3)\n");
#endif
      return(FALSE);
    };

    // is sensor p3 type ??
    if (cBufRead[0]==3)
    {
#ifdef DEBUG
     logPrintf("\r\r p3_reconnect OK\r\n");
#endif
     return(TRUE);
    }
    else
    {
     //printf("\r\rSensor is not P3 type, library support only p3 sensors \n");
#ifdef DEBUG
       dumpBuffer(cBufRead,5);
#endif 
     return(FALSE);
    }
  }

  // sensor is probably detacched from probe
  return (FALSE);
}

BOOL p3_eventcount(WORD *firstfree, BYTE *bank)
{
  #ifdef DEBUG
    printf("*** p3_eventcount ***\n");
  #endif
  
  strcpy(cmd,"14;48;65;85;85;85;0;0;0;85;85;85;85;85;23");
  if (cmdsim(cmd,16,TRUE,cBufRead)==FALSE)
  {
    printf("Cannot get info from p3_eventcount\n");
    return (FALSE);
  } 
  #ifdef DEBUG
    dumpBuffer(cBufRead,7);
  #endif
  *firstfree=cBufRead[4]+cBufRead[5]*256;
  *bank = cBufRead[6];
  return(TRUE);
}

BOOL _purgerxtx(void)
{
   ftStatus=FT_Purge(ftHandle[iDeviceIndex],FT_PURGE_RX || FT_PURGE_TX);
   if (ftStatus != FT_OK)
   {
     printf("[Erorr(%d), iDevIndex(%d)] Cannot purge RX/TX \n",(int)ftStatus,iDeviceIndex);
     return (FALSE);
   }
   return (TRUE);
}



// read all the data from sensor
// we reads 64bytes pages, result is in *memdump
BOOL p3_readtourwork(WORD firstfree, UCHAR *memdump)
{
  // state machine definition states
  typedef enum
  {
     bgSTART=0,  // purge rx/tx, then ask if p3 is attached and switching device to bgWRITE mode
     bgWRITE=1,  // sending order into TMD (and P3). After that we can switch to bgREAD case
     bgREAD=2,   // tmd is sending data collected from P3. 
     bgNOP3=3,   // device is not P3, we do not support P2 or datachip yet.
     bgTIMEOUT=4,// timeout error, the device doesnt respond in the expected time
     bgBUFOK=5,  // buffer checked for integrity, we can read next packet
     bgBUFERR=6, // buffer error, mostly if you dettach p3 for a while
     bgFINISH=7, // all went OK, we have fineshed run
     bgERROR=8,   // we cannot recover from this error
     bgRECONNECT=9
  } etw_state;
  static etw_state twstate;
 
  // variables 
  WORD lastpage,curaddr,curcnt;
  BYTE p3_missing,erRead,i;
  char firstbyte;
  WORD tmp;
  UCHAR *ptr;  // pointer to cBufRead; 

  //**********************************************/ 
  #ifdef DEBUG
   printf("\n**** p3_readtourwork ****\n"); 
  #endif
  
  // get the last page of the data  
  lastpage=(firstfree-1) / 64 +1;
  // check if last event fits exactly to 64 byte page and correct result
  if (((firstfree-1) % 64)==0)
    lastpage--;
  
  curcnt  = lastpage % 256;
  #ifdef DEBUG
    printf("lastpage=%d, curcount=%d \n",lastpage,curcnt);
  #endif
  
  twstate = bgSTART;
  p3_missing=0;
  do
  {
    #ifdef DEBUG
      printf("state=%d\n",(int)(twstate));
    #endif
    switch(twstate)
    { 
       /****************************************
       *** setup buffer and check presence of p3 on tmd ***
       ****************************************/
      case bgRECONNECT:
        //
        // is p3 available
        if (p3_reconnect()==FALSE)
        {
          #ifdef DEBUG
            printf("p3_reconnect() p3 missing\n");
          #endif
          p3_missing++;
          break;
        }
        twstate=bgSTART;
        
      case bgSTART:
        //printf("bgSTART\n");
	// clear hardware buffer on FTDI 
        if (_purgerxtx == FALSE)
        {
          printf("Cannot PurgeRxTx \n");
          twstate=bgERROR;
          break;
        }        
        /*
        // is p3 available
        if (p3_reconnect()==FALSE)
        {
          #ifdef DEBUG
            printf("p3_reconnect() p3 missing\n");
          #endif
          p3_missing++;
          break;
        }
        */
        // ok we can switch to bgWrite
        curaddr=0; 
	twstate=bgWRITE; 
        break;

      /**************************************************** 
       *** let the device know, we want guard tour data ***
       ****************************************************/ 
      case bgWRITE:
        #ifdef DEBUG
          printf("bgWrite %s\n",cmd);
        #endif
        sprintf(cmd,"4;$31;%d;%d;%d",HIGHBYTE(curaddr),LOWBYTE(curaddr),curcnt); 

        // test na kontrolu cteni stranky 
        //printf("bgWrite waiting before $31 \n");
        //while (getchar() != '\n') ;

        if (writebuf(cmd,FALSE)==TRUE)
        {
         erRead=0;
         twstate=bgREAD;
         break;
        }
        twstate=bgRECONNECT; 
        break;
      
      /***************************************************   
       *** get the data from TMD and check integrity     *
       ***************************************************/
      case bgREAD:
        #ifdef DEBUG 
          printf("bgRead \n");
        #endif
        
        // wait for the first byte in queue  
        if (readbuf(1,&firstbyte)==TRUE)            
        {
          // tmd adapter timeout, possibly because detaching p3 from tmd
          if ((firstbyte==254) || (firstbyte==255))
          {
            twstate= bgWRITE;
            break;
          }
        } 

        // read the rest 
        clearbuffer(cBufRead,BUF_SIZE);
        cBufRead[0]=firstbyte;
        if (readbuf(68,cBufRead+1)==FALSE)
        {
          // we got error
          twstate=bgWRITE; 
          break;
        }
        #ifdef FTDI_DEBUG
          dumpBuffer(cBufRead,69);
	#endif
        
        // couple of tests ensuring data integrity
        // if some of them for a reason fails, we get 
        // the same package from tmd WITHOUT calling bgWRITE state
        // maximum number of repaired packets getting from TMD is 3x
        twstate=bgBUFOK; 

        // bad package has usually 0x02 here
        if (cBufRead[68] !=0) 
        {
          printf("cBufRead[68]=%d and should be 0\n",cBufRead[68]);
          
          clearbuffer(cBufRead,BUF_SIZE);
          if (readbuf(69,cBufRead)==FALSE)
          {
            twstate=bgWRITE;
            break;
          }
        } 

        if (cBufRead[68] !=0)
        {
          printf("cBufRead[68]=%d and should be 0\n",cBufRead[68]); 
          clearbuffer(cBufRead,BUF_SIZE);
          if (readbuf(69,cBufRead)==FALSE)
          {
            twstate=bgWRITE;
            break;
          }
        }
       
        if (cBufRead[68] !=0)
        {
          twstate=bgBUFERR;
          break;
        }
 
        
        // check if adapter returned correct curaddr
        tmp=(cBufRead[0]*256+(cBufRead[1] & 0xFE));
        if (tmp!=curaddr)
        { 
          printf("curaddr(%d)<>tmp(%d)\n",curaddr,tmp);
          twstate=bgBUFERR; 
        } 
        // check for bank
        if ((cBufRead[1] & 0x01) !=0)
        {
          printf("bank(%d) AND= %d \n",cBufRead[1],cBufRead[1] && 0x01);
          twstate=bgBUFERR;
        } 
	break;

      /**************************************************
       *** bgREAD went OK, we can manage counters       *
       **************************************************/
      case bgBUFOK:
 
        // copy data to big field (Miron approach)
        #ifdef DEBUG 
          // printf("\ncurraddr=");
        #endif 
        for (i=0;i<64;i++)
        {
         #ifdef DEBUG
         //  printf("%d,",i+curaddr);
         #endif
         memdump[i+curaddr] = cBufRead[i+2];
        }
        curcnt--;  // can safely decrease number of 64 byte pages
        curaddr=curaddr+64; 


        // allow state machine read next packet
        if (curcnt>0)
          twstate=bgREAD;
        else
          twstate=bgFINISH;
        break;

      /**************************************************
       *** Error occured during bgREAD state, we could  *
       *** try read packet 3x times, after that we      *
       *** should write the order again                 *
       **************************************************/
      case bgBUFERR:
        
         /* 
        // Buffer error, check if we need call bgWRITE again
        if (erRead<=3)
          twstate= bgREAD;
        else 
          twstate=bgWRITE; 
        */
        twstate=bgERROR;
        printf("bgBUFERR\n");
        break;

      case bgNOP3:
        printf("bgNop3\n");
        break;

      case bgFINISH:
        printf("bgFINISH\n");
        break;
      case bgERROR:
        printf("bgERROR \n");
        break;
      default:
        printf("error state \n");
        break;
    }
  } while ((twstate!=bgFINISH) && (twstate!=bgERROR));

  #ifdef DEBUG
   printf("*** finish read_tourwork() ***\n");
  #endif

  return(twstate==bgFINISH);
}

// we can reads settings in 8 byte chunks
// it is helpers function for p3_readeeprom below
BOOL p3_eechunk(BYTE addr, BYTE count, UCHAR *id8)
{
  BYTE i;
  BOOLEAN res;
  res = FALSE;
  do
  {
    //clearbuffer(cBufRead,BUF_SIZE);
    //res = p3_reconnect();
    sprintf(cmd,"14;$30;$11;%d;0;%d;0;0;0;0;0;0;0;0;0;255",count-1,addr);
    res=cmdsim(cmd,16,TRUE,cBufRead);  // tady ma by TRUE
    if (res)
    {
      if (cBufRead[15] != 0)
      {
        #ifdef DEBUG_FTDI
         printf("cbufRead[15]<>0");
         dumpBuffer(cBufRead,16);
        #endif
        if (p3_reconnect() == FALSE)
         return(FALSE);
      }
    }
  } while (res != TRUE);
  
  // copy buffer to id8
  for (i=0;i<8;i++)
    id8[i]=cBufRead[i+4];

  #ifdef DEBUG_FTDI
   printf("\np3_eechunk addr=%d  result=%d\n",addr,res);
   dumpBuffer(id8,8);
  #endif
  return (res);
}

// result in global variable p3header
BOOL p3_getheader(UCHAR *mem)
{
  BYTE i;
  
  p3header.serial = 0;
  for (i=4;i>0;i--)
  {
    p3header.serial = p3header.serial * 256;
    p3header.serial = p3header.serial + mem[0x0C+i-1];
  }
  #ifdef TDEB
   printf("serial =%d \n",p3header.serial);
  #endif
}


// READ setttings from internal EEPROM
BOOL p3_readeeprom(void)
{
 BYTE i,j;
 BYTE  mem[16*8];
 UCHAR id8[8];  
 unsigned int StartTime; 
 BOOLEAN res;

 clearbuffer(mem,16*8);
 StartTime = GetTickCount();
 i=0;
 do
 {
   res = p3_eechunk(i*8,8,id8); 
   if (res)
   {
     for (j=0;j<8;j++)
      mem[i*8+j]=id8[j];

     i++;
   }
   else 
     return(FALSE);
  
 } while (i<14) ;   

 #ifdef DEBUG_FTDI
  printf("\r\r p3_readeeprom \r\n");
  dumpBuffer(mem,14*8);
 #endif
 
 p3_getheader(mem) ;
 return (TRUE);
}

BOOL  p3_beep_par(BYTE par)
{
	BOOL ret;

	sprintf(cmd, "14;$30;$%d;$55;$55;$55;$55;$55;$55;$55;$55;$55;$55;$55", par);
	//strcpy(cmd, "14;$30;$06;$55;$55;$55;$55;$55;$55;$55;$55;$55;$55;$55");
	ret = cmdsim(cmd, 16, TRUE, cBufRead);
	if (ret == FALSE)
		printf("Cannot beep common in P3 \n");
	return (ret);
}

BOOL  p3_beep_common(void)
{
   BOOL ret;

   strcpy(cmd,"14;$30;$02;$55;$55;$55;$55;$55;$55;$55;$55;$55;$55;$55");
   ret=cmdsim(cmd,16,TRUE,cBufRead);
   if (ret==FALSE)
     printf("Cannot beep common in P3 \n");
   return (ret);
}

BOOL  p3_beep_ok(void)
{
   BOOL ret;

   // misto 07 (OK beep): 02 (common beep)
   strcpy(cmd,"14;$30;$07;$55;$55;$55;$55;$55;$55;$55;$55;$55;$55;$55");
   ret=cmdsim(cmd,16,TRUE,cBufRead);
   if (cBufRead[15]==2)
   {
     ret=p3_reconnect();
     #ifdef FTDI_DEBUG
       dumpBuffer(cBufRead,16);
     #endif
     return (FALSE);
   }
   if (ret==FALSE)
     printf("Cannot beep ok in P3 \n");
   return (ret);
}


BOOL  p3_delete(void)
{
   BOOL ret;

   strcpy(cmd,"14;$30;$40;$01;$55;$55;0;0;0;$55;$55;$55;$55;$55");
   ret=cmdsim(cmd,16,TRUE,cBufRead);
   if (cBufRead[15]>0)
   { 
     #ifdef DEBUG
       printf("\n p3_delete: ");
       dumpBuffer(cBufRead,16);
     #endif
     ret=p3_reconnect(); 
     return (FALSE);
   }

   if (ret==FALSE)
     printf("Cannot delete data in P3 \n");

   return (ret);
}

BOOL p3_pointer(void)
{
   BOOL ret;
   strcpy(cmd,"14;$30;$41;$00;$55;$55;0;0;0;$55;$55;$55;$55;$55");
   ret=cmdsim(cmd,16,TRUE,cBufRead);
   if (ret==FALSE)
        printf("Cannot delete data in P3 \n");
   //dumpBuffer(cBufRead,16);
   return (ret);
}


BYTE ByteToBCD(BYTE byt)
{
  // 53 -> 0x53
  BYTE ret;
  ret= byt % 10 + (((byt / 10) &  0x0F)<<4);
}

int BCDToByte(BYTE byt)
{
  int  ret;
  //ret = (byt % 16) + (byt / 16 ) *10;
  ret = (byt / 16) * 10;
  ret = byt % 16+ ret;
  return(ret);
}


BOOL p3_readtime(void)
{
  BOOL ret;
  static struct tm t;
  BYTE start;

  //printf("***** read time *****************************************************\r\n");
  sprintf(cmd,"14;$30;$21;$00;$55;$55;$00;$00;$00;$00;$00;$00;$00;$55");
  
  ret = cmdsim(cmd,16,TRUE,cBufRead);
  if (cBufRead[15]>0)
  {
     logPrintf("p3_readtime cbufRead[15]=%2x\r\n",cBufRead[15]);
     #ifdef DEBUG
        dumpBuffer(cBufRead,16);
     #endif
	 // do
	 // {
		  ret = p3_reconnect();    
	  //} while (ret==FALSE); 
     return (FALSE);
  }
  if (!ret)
    return(FALSE);
  
  #ifdef DEBUG
    printf("p3_readtime OK\r\n");
    dumpBuffer(cBufRead,16);

    int year = BCDToByte(cBufRead[10]);
    printf("vzor: 0x%02X rok: %d\r\n",cBufRead[10],year);

    BYTE j,b;
    for (int i=4;i<=10;i++)
    {
      j = cBufRead[i];
      b = BCDToByte(j);
      printf("i:%d val:0x%02X bcd: %d\r\n",
              i,cBufRead[i],b);
    };
  #endif
  start = 4;

  t.tm_year = BCDToByte(cBufRead[start+6]) +100 ;
  t.tm_mon  = BCDToByte(cBufRead[start+5] & 0x1F) -1 ;
  t.tm_wday = BCDToByte(cBufRead[start+4] & 0x07);
  t.tm_mday = BCDToByte(cBufRead[start+3] & 0x3F); 
  t.tm_hour = BCDToByte(cBufRead[start+2] & 0x3F);
  t.tm_min  = BCDToByte(cBufRead[start+1] & 0x7F);
  t.tm_sec  = BCDToByte(cBufRead[start+0] & 0x7F);
  logPrintf("p3 time %02d:%02d:%02d %04d.%02d.%02d\r\n",t.tm_hour,t.tm_min,t.tm_sec,
	  t.tm_year+1900,t.tm_mon,t.tm_mday);

  time_t p3time,curtime;
  struct tm *loctime;

  p3time = mktime(&t); 
  curtime= time(NULL);

  char cal_string[30];
  
  strftime(cal_string,30,"%Y.%m.%d - %H:%M:%S,",  &t);
  printf(cal_string);

  double diff = difftime(p3time,curtime);
  diff = diff / 60 / 60;  // prevedu na hodiny	
  int hours = (diff  );

  logPrintf("diff: %3.3f hour:%d\r\n",diff,hours);
  if (abs(hours)<5)
    return(TRUE);

  return(FALSE);
}



BOOL p3_settime(void)
{
  time_t current;
  struct tm *t;
  unsigned char bcds[2];
  unsigned char tcmd[20];
  BOOL ret;

  current = time(NULL);
  /* 
  printf("Date and Time are %s\n", ctime(&current));
  printf("Date and Time are %s\n", asctime(localtime(&current)));
  */
  t = localtime(&current);
  logPrintf("Date is %d/%d/%d %d:%d:%d\r\n", t->tm_mon+1, t->tm_mday, t->tm_year + 1900,t->tm_hour,t->tm_min,t->tm_sec);
  
  // shift the day 
  if (t->tm_wday==0) (t->tm_wday=7);
  sprintf(cmd,"14;$30;$20;$55;$55;$55;$%02X;$%02X;$%02X;$%02X;$%02X;$%02X;$%02X;$%02X",
  ByteToBCD(t->tm_sec),ByteToBCD(t->tm_min),ByteToBCD(t->tm_hour),
  ByteToBCD(t->tm_mday), ByteToBCD(t->tm_wday),
  ByteToBCD(t->tm_mon+1),ByteToBCD(t->tm_year-100),0x55);
  //printf("%s\n",cmd); 
  
  ret=cmdsim(cmd,16,TRUE,cBufRead);
   /*
   if (ret==FALSE)
     printf("Cannot set time in P3 \n");
   return (ret);
   */
	return (ret);
}

