/*
This file is part of PS3FTP
(a minimal PlayStation 3 FTP Server)

PS3FTP is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

PS3FTP is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with PS3FTP.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <net/net.h>
#include <io/pad.h> 

#include <sysmodule/sysmodule.h>
#include <lv2/process.h>

#include "helper.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>

#include <psl1ght/lv2/filesystem.h>

#define FTP_PORT 21
#define MAX_LINE 10000

int ftp_s, ftp_c; // ftp control sockets
int data_s, data_c; // ftp data sockets
u32 rest = 0; // ftp REST value

char cwd[MAX_LINE]; // current working directory
char new_cwd[MAX_LINE]; // used during a change of the working directory (CWD)
char rename_from[2048]; // used in the "rename from" command (RNFR)
char request[MAX_LINE]; // string for reading requests
char temp[MAX_LINE]; // string for temporary values and responses

void readRequest(int fd)
{
	Readline(fd, request, MAX_LINE-1);
	printf("request: %s\r\n", request);
}

void openSocket(int port, int* client_s, int* server_s, int reuseable)
{
	if((*server_s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("ECHOSERV: Error creating listening socket. (%d)\r\n", libnet_errno);
		exit(EXIT_FAILURE);
	}

	//setsockopt(*server_s, SOL_SOCKET, SO_REUSEADDR, &reuseable, sizeof(int));

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(port);

	printf("listening to port %d...\r\n", port);

	if(bind(*server_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
	{
		printf("ECHOSERV: Error calling bind()\r\n");
		exit(EXIT_FAILURE);
	}

	if( listen(*server_s, LISTENQ) < 0)
	{
		printf("ECHOSERV: Error calling listen()\r\n");
		exit(EXIT_FAILURE);
	}

	if((*client_s = accept(*server_s, NULL, NULL) ) < 0)
	{
		printf("ECHOSERV: Error calling accept()\r\n");
		exit(EXIT_FAILURE);
	}
	else
		printf("connection at port %d accepted\r\n", port);
}

void closeSocket(int socket)
{
	if(closesocket(socket) < 0)
	{
		printf("ECHOSERV: Error calling close()\r\n");
		exit(EXIT_FAILURE);
	}
	else
		printf("socket closed...\r\n");
}

int xPressed()
{
	PadInfo padinfo;
	PadData paddata;

	ioPadInit(7);
	ioPadGetInfo(&padinfo);

	int i, xPressed = 0;
	for(i=0; i<MAX_PADS; i++)
	{
		if(padinfo.status[i])
		{
			ioPadGetData(i, &paddata);
			xPressed |= paddata.BTN_CROSS;
		}
	}

	ioPadEnd();
	return xPressed;
}

int exists(char* path)
{
	struct stat entry; 
	return stat(path, &entry);
}

int isDir(char* path)
{
	struct stat entry; 
	stat(path, &entry);
	return ((entry.st_mode & S_IFDIR) != 0);
}

void absPath(char* absPath, const char* path)
{
	if(strlen(path) > 0 && path[0] == '/')
		strcpy(absPath, path);

	else
	{
		strcpy(absPath, cwd);
		strcat(absPath, path);
	}
}

#include "cmds.h"

int main(int argc, char *argv[])
{
	SysLoadModule(SYSMODULE_NET);
	net_initialize_network();

	printf("Starting PS3 FTP Server v1.0 - Build: %s\r\n", __DATE__);

	while(1)
	{
		strcpy(cwd, "/");
		printf("New FTP session started...\r\n");

		openSocket(FTP_PORT, &ftp_s, &ftp_c, 0);
		WritelineEx(ftp_s, "220 Hi, I'm your PS3 FTP server.\r\n", 1);

		while(Readline(ftp_s, request, MAX_LINE-1) > 0 && strncmp(request, "QUIT", 4) != 0)
		{
			printf("Request: %s\r\n", request);

			if(strncmp(request, "USER", 4) == 0)
				cmd_USER();

			else if(strncmp(request, "PASS", 4) == 0)
				cmd_PASS();

			else if(strncmp(request, "FEAT", 4) == 0)
				WritelineEx(ftp_s, "211-Currently no extensions supported\r\n211 END\r\n", 1);

			else if(strncmp(request, "PWD", 3) == 0)
				cmd_PWD();

			else if(strncmp(request, "PASV", 4) == 0)
				cmd_PASV();

			else if(strncmp(request, "LIST", 4) == 0)
				cmd_LIST();

			else if(strncmp(request, "RETR", 4) == 0)
				cmd_RETR();

			else if(strncmp(request, "CWD", 3) == 0)
				cmd_CWD();

			else if(strncmp(request, "CDUP", 4) == 0)
				cmd_CDUP();

			else if(strncmp(request, "TYPE ", 5) == 0)
				WritelineEx(ftp_s, "200 Type set to SOMETHING!!!\r\n", 1);

			else if(strncmp(request, "SYST", 4) == 0)
				WritelineEx(ftp_s, "211 PlayStation3 Type: L8\r\n", 1);

			else if(strncmp(request, "REST", 4) == 0)
				cmd_REST();

			else if(strncmp(request, "DELE", 4) == 0)
				cmd_DELE();

			else if(strncmp(request, "STOR", 4) == 0)
				cmd_STOR();

			else if(strncmp(request, "MKD", 3) == 0)
				cmd_MKD();

			else if(strncmp(request, "RMD", 3) == 0)
				cmd_RMD();

			else if(strncmp(request, "RNFR", 4) == 0)
				cmd_RNFR();

			else if(strncmp(request, "RNTO", 4) == 0)
				cmd_RNTO();

			else if(strncmp(request, "SIZE", 4) == 0)
				cmd_SIZE();

			else
			{
				WritelineEx(ftp_s, "500 command not recognized\r\n", 1);
				printf("UNKNOWN command: %s\r\n", request);
			}
		}

		WritelineEx(ftp_s, "200 Byebye!\r\n", 1);

		closeSocket(ftp_s);
		closeSocket(ftp_c);

		printf("FTP session closed...\r\n");

		if(strncmp(request, "QUIT!", 5) == 0 || xPressed() != 0)
			break; // shutdown
		else if(strncmp(request, "QUITR", 5) == 0)
		{
			sysProcessExitSpawn2("/dev_hdd0/ps3load.self", NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
			break; // shutdown
		}
	}

	printf("Shutting down PS3 FTP Server.\r\n");

	net_finalize_network();
	SysUnloadModule(SYSMODULE_NET);
}
