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

#include <sysmodule/sysmodule.h>
#include <lv2/process.h>

#include "helper.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>

#include <psl1ght/lv2/filesystem.h>

#define FTP_PORT           21
#define MAX_LINE           10000
char                       buffer[MAX_LINE];

void readRequest(int fd)
{
	memset(buffer, 0, MAX_LINE);
	Readline(fd, buffer, MAX_LINE-1);
	printf("request: %s\r\n", buffer);
}

void openSocket(int port, int* client_s, int* server_s, int reuseable)
{
	if ( (*server_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
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

	if ( bind(*server_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
		printf("ECHOSERV: Error calling bind()\r\n");
		exit(EXIT_FAILURE);
	}

	if ( listen(*server_s, LISTENQ) < 0 ) {
		printf("ECHOSERV: Error calling listen()\r\n");
		exit(EXIT_FAILURE);
	}

	if ( (*client_s = accept(*server_s, NULL, NULL) ) < 0 ) {
		printf("ECHOSERV: Error calling accept()\r\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("connection at port %d accepted\r\n", port);
	}
}

void closeSocket(int socket)
{
	if(socketclose(socket) < 0)
	{
		printf("ECHOSERV: Error calling close()\r\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("socket closed...\r\n");
	}
}

int ftp_s, ftp_c;
int data_s, data_c;
u32 rest = 0;

char cwd[2048];

#include "cmds.h"

int main(int argc, char *argv[])
{
	SysLoadModule(SYSMODULE_NET);
	net_initialize_network();

	printf("Starting PS3 FTP Server.\r\n");

	while(1)
	{
		strcpy(cwd, "/");
		printf("New FTP session started...\r\n");

		openSocket(FTP_PORT, &ftp_s, &ftp_c, 0);
		WritelineEx(ftp_s, "220 Hi, I'm your PS3 FTP server.\r\n", 1);

		while(Readline(ftp_s, buffer, MAX_LINE-1) > 0 && strncmp(buffer, "QUIT", 4) != 0)
		{
			printf("Request: %s\r\n", buffer);

			if(strncmp(buffer, "USER", 4) == 0)
				cmd_USER(buffer);

			else if(strncmp(buffer, "PASS", 4) == 0)
				cmd_PASS(buffer);

			else if(strncmp(buffer, "FEAT", 4) == 0)
				WritelineEx(ftp_s, "211-Currently no extensions supported\r\n211 END\r\n", 1);

			else if(strncmp(buffer, "PWD", 3) == 0)
				cmd_PWD(buffer);

			else if(strncmp(buffer, "PASV", 4) == 0)
				cmd_PASV(buffer);

			else if(strncmp(buffer, "LIST", 4) == 0)
				cmd_LIST(buffer);

			else if(strncmp(buffer, "RETR", 4) == 0)
				cmd_RETR(buffer);

			else if(strncmp(buffer, "CWD", 3) == 0)
				cmd_CWD(buffer);

			else if(strncmp(buffer, "CDUP", 4) == 0)
				cmd_CDUP(buffer);

			else if(strncmp(buffer, "TYPE ", 5) == 0)
				WritelineEx(ftp_s, "200 Type set to SOMETHING!!!\r\n", 1);

			else if(strncmp(buffer, "SYST", 4) == 0)
				cmd_SYST(buffer);

			else if(strncmp(buffer, "REST", 4) == 0)
				cmd_REST(buffer);

			else if(strncmp(buffer, "DELE", 4) == 0)
				cmd_DELE(buffer);

			else if(strncmp(buffer, "STOR", 4) == 0)
				cmd_STOR(buffer);

			else if(strncmp(buffer, "MKD", 3) == 0)
				cmd_MKD(buffer);

			else if(strncmp(buffer, "RMD", 3) == 0)
				cmd_RMD(buffer);

			else if(strncmp(buffer, "RNFR", 4) == 0)
				cmd_RNFR(buffer);

			else if(strncmp(buffer, "RNTO", 4) == 0)
				cmd_RNTO(buffer);

			else
			{
				WritelineEx(ftp_s, "500 command not recognized\r\n", 1);
				printf("UNKNOWN command: %s\r\n", buffer);
			}
		}

		WritelineEx(ftp_s, "200 Byebye!\r\n", 1);

		closeSocket(ftp_s);
		closeSocket(ftp_c);

		printf("FTP session closed...\r\n");

		if(strncmp(buffer, "QUIT!", 5) == 0)
		{
			return 0;
		}
		else if(strncmp(buffer, "QUITR", 5) == 0)
		{
			sysProcessExitSpawn2("/dev_hdd0/ps3load.self", NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);
			return 0;
		}
	}

	printf("Shutting down PS3 FTP Server.\r\n");

	SysUnloadModule(SYSMODULE_NET);
	net_finalize_network();
}
