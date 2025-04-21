#include "cser.hh"
#include <iostream>
#include <string.h>
#include <stdio.h>

using namespace std;



int main(int argc, char*argv[])
{
	bool ret;
	std::string text;
	
	if ((argc > 1) && (argv[1][0] == 'T'))
	  return (1);
  
	Csr *srv = new Csr;
	
	/*
	srv->test_jsonwrite("stim.json");
	printf("FIN\r\n");
	exit(0);
	*/
	
	/*
	if (srv->test_msg("CFF"))
		printf("%s\r\n", srv->ipc_msg());
	exit(0);
	*/
	
	/*
	srv->send_json();
	exit(0);
    */
	
	srv->setMachine(m_start);
	srv->mLoop();
	
	printf("Fin\r\n");
}
