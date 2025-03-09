#include "iniparser.h"
#include "asmjs.h"
#include <string.h>

int parse_ini_file(char * ini_name)
{
    dictionary  *ini ;

    /* Some temporary variables to hold query results */
    int             b ;
    int             i ;
    double          d ;
    char  *   s ;
	char p[18];
      
    //int _auto_user;
   // char _host[16];
   // int _port;

    ini = iniparser_load(ini_name);
    if (ini==NULL) {
        fprintf(stderr, "cannot parse file: %s\n", ini_name);
        return -1 ;
    }
    iniparser_dump(ini, stderr);
	//int parse_ini_file(char * ini_name);
    _auto_user = iniparser_getint(ini, ":_auto_user", -1);
	_port =  iniparser_getint(ini, ":_port", -1);
	s = iniparser_getstring(ini, ":_host", NULL);
	strcpy(_host, s);

	
   iniparser_freedict(ini); 
   return (0);	 
}
