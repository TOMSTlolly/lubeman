#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>
#include <unistd.h>

#define PROC_PATH "/proc/"
#define CMDLINE_FILE "/cmdline"

#define MAX_PROCESSES 1024
#include "tcpx.h"
#include "asmjs.h"

bool run_process(const char *process_name)
{
	bool ret = false;
	char command[256];
	snprintf(command, sizeof(command), "%s &", process_name);
	int result = system(command);

	if (result == -1) {
		perror("Failed to execute pkill");
		return false;
	}
}

bool stop_proc(const char *process_name)
{
	bool ret = false;
	char command[256];
	snprintf(command, sizeof(command), "pkill %s", process_name);
	int result = system(command);

	if (result == -1) {
		perror("Failed to execute pkill");
		return false;
	}
}

int get_pids_by_namex(const char *process_name, int *pids, int max_pids) {
	FILE *fp;
	char command[256];
	snprintf(command, sizeof(command), "pgrep %s", process_name);

	fp = popen(command, "r");
	if (fp == NULL) {
		perror("Failed to run pgrep");
		return -1;
	}

	// vymaz buffer
	for (int i = 0; i < max_pids; i++) {pids[0]=0;}

	int count = 0;
	while (fscanf(fp, "%d", &pids[count]) != EOF && count < max_pids) {
		printf("%s found pid %d \n",process_name,pids[count]);
		count++;
	}

	pclose(fp);
	return count;
}

bool stop_process(const char *process_name)
{
	bool ret = false;
	int pids[256];
	int ir=0;

	ir = get_pids_by_namex(process_name, pids, 256);
	if (ir<0)
	{
		// chyba
		printf("process %s not found \r\n",process_name);
		return false;
	}

	ret = stop_proc(process_name);
	ir = get_pids_by_namex(process_name, pids, 256);
	if (ir>-1)
	{
		// chyba
		printf("process %s not killed \r\n",process_name);
		return false;
	}


	int num_pids = get_pids_by_namex(process_name, pids, 256);
	if (num_pids<1)
	{
		printf("process %s not found \r\n",process_name);
		return false;
	}

	for (int i =0; i < num_pids; i++)
	{
		kill(pids[i], SIGKILL);  // Terminate the process
		sleep(0.1);            // Wait before restar
	};

	for (int i=0;i<256;i++) pids[i] = 0;
	num_pids = get_pids_by_namex(process_name, pids, 256);
	if (num_pids<=0)
	{
		printf("all killing went fine :-)\r\n");
		return false;
	}

	for (int i = 0; i < num_pids; i++)
	{
		printf("still lives %d \r\n",pids[i]);
		ret = false;
	}
	return true;
}

// cekej v loopu na povel k zabiti obou procesu
bool should_kill(char *ip, int port)
{
	char par[16];
	bool ret = false;

	//restart_process(pid,process_name);
	char cmd[128];
	char response[128];


	strcpy(cmd,"GDA");
	strcpy(par,"USERNAME");
	if (writetcp(ip,port,cmd,par,response) != 0)
	{
		printf("chyba pri spojeni \r\n");
		return (EXIT_FAILURE);
	}
	//printf("packet sent to %s:%d resp=%s",ip,port,response);
	if (strstr(response,"NCK") != NULL)
	{
		printf("aleapon>1 min neni spojeni s krabici %s\r\n",ip);
		return (true); // ANO ZABIJ OBA PROCESY
	}
	return (false);

}

int main(int argc, char *argv[]) {
	char ip[20];
	char dir[256],path[256],exe[256];
	int  port=5000;
	bool  kilall=false;
	bool debug_mode = false;

	for (int i = 0; i < argc; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--ip") == 0 && i + 1 < argc) {
			strncpy(ip, argv[i + 1], sizeof(ip) - 1);
			ip[sizeof(ip) - 1] = '\0';
			i++; // Skip the next argument as we've used it
		} else if (strcmp(argv[i], "--port") == 0 && i +1  < argc) {
			port = atoi(argv[i + 1]);
			i++; // Skip the next argument
		} else if (strcmp(argv[i], "--kill") == 0 && i + 1 < argc) {
			kilall = atoi(argv[i +1 ])>0;
			i++; // Skip the next argument
		}else if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
			strncpy(dir, argv[i], sizeof(dir) - 1);
			dir[sizeof(dir) - 1] = '\0';
			i++;
		} else if (strcmp(argv[i], "--debug") == 0) {
			debug_mode = true;
		} else if (strcmp(argv[i], "--help") == 0) {
			printf("Usage: %s [OPTIONS]\n", argv[0]);
			printf("Options:\n");
			printf("  --ip <address>     Server IP address (default: 127.0.0.1)\n");
			printf("  --port <number>    Server port (default: 5000)\n");
			printf("  --dir <path>       Directory path (default: /home/krata/lubeman/)\n");
			printf("  --kill             Kill simple and pyguard.py processes\n");
			printf("  --debug            Enable debug mode\n");
			printf("  --help             Display this help message\n");
			return EXIT_SUCCESS;
		}
	}

	if (kilall)
	{
		stop_process("simple");
		stop_process("pyguard.py");

		sleep(0.5);
		exit(0);
	}

	bool ret=false;
	//strcpy(ip,"10.0.0.146");

	// stopni bezici procesy a restartuj pres shell
	char cwd[64];
	if (!getcwd(cwd,sizeof(cwd)) != NULL)
	{
		perror("getcwd error \r\n");
		exit(0);
	}
	//strcpy(dir,"/home/krata/lubeman/");
	//strcat(cwd,"/");
	//strcpy(dir,cwd);
	strcpy(dir,"/home/pi/lubeman/");
	strcpy(path,dir);
	strcat(path,"lan_reader_cfg.txt");
	//parse_ini_file("/home/krata/lubeman/lan_reader_cfg.txt");
	parse_ini_file(path);
	strcpy(ip,_host);
	port = _port;
        
	ret = stop_process("simple");
	ret = stop_process("pyguard.py");
	while (true)
	{
		if (should_kill(ip,port) == true)
		{
			// vypni procesy
			printf("should_kill() true \r\n");
			ret = stop_process("simple");
			ret = stop_process("pyguard.py");

			//return (EXIT_FAILURE);
			sleep(1);

			strcpy(exe,dir);
			strcat(exe,"pyguard.py");
			printf("********* RESTART ********* \r\n");
			run_process(exe);
			printf("********* RESTARTED ********* \r\n");
			sleep(5);
		}
		sleep(5);
	}
	return F_OK;
}
