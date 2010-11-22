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

#pragma once

#define DATA_CON_BUF_LEN 512

int cmd_USER(char* request)
{
	WritelineEx(ftp_s, "331 user accepted\r\n", 1);
	return 0;
}

int cmd_PASS(char* request)
{
	WritelineEx(ftp_s, "230 password accepted\r\n", 1);
	return 0;
}

int cmd_PWD(char* request)
{
	char temp[2048];
	strcpy(temp, "257 \"");
	strcat(temp, cwd);
	strcat(temp, "\"\r\n");
	WritelineEx(ftp_s, temp, 1);
	return 0;
}

int cmd_PASV(char* request)
{
	rest = 0;
	net_sockinfo_t snf;

	net_get_sockinfo(ftp_s, &snf, 1); // 1 entry

	char response[2048];
	sprintf(response, "227 Entering Passive Mode (%u,%u,%u,%u,240,206)\r\n", 
			(snf.local_adr.s_addr & 0xFF000000) >> 24, 
			(snf.local_adr.s_addr & 0xFF0000) >> 16, 
			(snf.local_adr.s_addr & 0xFF00) >> 8, 
			(snf.local_adr.s_addr & 0xFF));

	WritelineEx(ftp_s, response, 1);

	openSocket(61646, &data_s, &data_c, 1);
	return 0;
}

int cmd_LIST(char* request)
{
	WritelineEx(ftp_s, "150 Opening connection\r\n", 1);

	int root;
	if(lv2FsOpenDir(cwd, &root) == 0)
	{
		uint64_t read = -1;
		Lv2FsDirent ent;

		WritelineEx(data_s, "drw-rw-rw-   1 root  root        512 Jan 01  2010 ..\r\n", 0);

		lv2FsReadDir(root, &ent, &read);

		while(read != 0)
		{
			char path[2048];
			strcpy(path, cwd);
			strcat(path, "/");
			strcat(path, ent.d_name);

			struct stat entry; 
			stat(path, &entry);

			char temp[2048];
			if((entry.st_mode & S_IFDIR) != 0)
				strcpy(temp, "d");
			else
				strcpy(temp, "-");

			strcat(temp, "rw-rw-rw-   1 root  root        ");
			char sz_bf[32];
			sprintf(sz_bf, "%lu", entry.st_size);
			strcat(temp, sz_bf);
			strcat(temp, " Jan 01  2010 ");
			strncat(temp, ent.d_name, ent.d_namlen);
			strcat(temp, "\r\n");
			Writeline(data_s, temp, strlen(temp));

			if(lv2FsReadDir(root, &ent, &read) != 0)
			{
				printf("Error while reading directory entry!!!\n");
				break;
			}
		}

		lv2FsCloseDir(root);
	}

	WritelineEx(ftp_s, "226 Transfer Complete\r\n", 1);
	closeSocket(data_c);
	closeSocket(data_s);
	return 0;
}

int cmd_RETR(char* request)
{
	WritelineEx(ftp_s, "150 Opening BINARY connection\r\n", 1);
	char filename[2048];
	strcpy(filename, cwd);
	strcat(filename, "/");
	strcat(filename, request+5);

	char buf[DATA_CON_BUF_LEN];
	int rd = -1, wr = -1;
	int fd = open(filename, O_RDONLY);

	lseek(fd, rest, SEEK_SET);

	while((rd = read(fd, buf, DATA_CON_BUF_LEN)) > 0)
	{
		wr = send(data_s, buf, rd, 0);
		if(wr != rd)
			break;
	}

	close(fd);

	if(rd < 1)
		rd = wr = 0;

	char temp[2048];
	sprintf(temp, "%d %s ... %s\r\n", 
		(wr == rd)?226:426, 
		filename, 
		(wr == rd)?"Transfer Complete":"Transfer aborted");

	WritelineEx(ftp_s, temp, 1);
	closeSocket(data_c);
	closeSocket(data_s);
	return 0;
}

int cmd_CWD(char* request)
{
	char new_cwd[2048];

	// change relative to root directory
	if(request[4] == '/')
	{
		// back to the root
		if(strlen(request) == 5)
			strcpy(new_cwd, "/");
		// requested full path
		else
			strcpy(new_cwd, request+4);
	}
	// change relative to current CWD
	else
	{
		strcpy(new_cwd, cwd);
		strcat(new_cwd, request+4);
	}

	if(new_cwd[strlen(new_cwd)-1] != '/')
		strcat(new_cwd, "/");

	struct stat entry; 
	stat(new_cwd, &entry);

	char response[2048];

	if((entry.st_mode & S_IFDIR) != 0)
	{
		sprintf(response, "250 new pwd: %s\r\n", new_cwd);
		strcpy(cwd, new_cwd);
	}
	else
		sprintf(response, "550 Could not change directory: %s\r\n", new_cwd);

	WritelineEx(ftp_s, response, 1);
	return 0;
}

int cmd_CDUP(char* request)
{
	char temp[2048];
	strcpy(temp, "250 new pwd: ");

	for(int i=strlen(cwd)-2; i>0; i--)
	{
		if(cwd[i] != '/')
		{
			cwd[i] = '\0';
		}
		else
			break;
	}

	strcat(temp, cwd);
	strcat(temp, "\r\n");

	WritelineEx(ftp_s, temp, 1);
	return 0;
}

int cmd_SYST(char* request)
{
	WritelineEx(ftp_s, "PlayStation_3 Type: L8\r\n", 1);
	return 0;
}

int cmd_REST(char* request)
{
	char num[16];
	strcpy(num, request+5);
	rest = atoi(num);
	printf("rest set to %u\n", rest);
	WritelineEx(ftp_s, "200 rest set\r\n", 1);
	return 0;
}

void cmd_DELE(char* request)
{
	char path[2048];
	strcpy(path, cwd);
	strcat(path, request+5);
	printf("remove %s = %d\n", path, remove(path));
	WritelineEx(ftp_s, "250 DELE command successful.\r\n", 1);
}

// STOR_/
void cmd_STOR(char* request)
{
	WritelineEx(ftp_s, "150 Opening BINARY connection\r\n", 1);

	char path[2048];
	if(request[5] == '/')
		strcpy(path, "");
	else
		strcpy(path, cwd);

	strcat(path, request+5);

	char buf[DATA_CON_BUF_LEN];
	int rd = -1, wr = -1;
	int fd = open(path, O_WRONLY | O_CREAT);

	printf("opening file %s ... %d\n", path, fd);

	if(fd > 0)
	{
		while((rd = recv(data_s, buf, DATA_CON_BUF_LEN, 0)) > 0)
		{
			wr = write(fd, buf, rd);
			if(wr != rd)
				break;
		}

		if(rd <= 0)
			wr = rd;

		close(fd);
	}
	else
		wr = 1;

	char temp[2048];
	sprintf(temp, "%d %s ... %s\r\n", 
		(wr == rd)?226:426, 
		path, 
		(wr == rd)?"Transfer Complete":"Transfer aborted");

	WritelineEx(ftp_s, temp, 1);
	closeSocket(data_c);
	closeSocket(data_s);
}

void cmd_MKD(char* request)
{
	char path[2048];
	strcpy(path, cwd);
	strcat(path, request+4);
	printf("create %s = %d\n", path, mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));
	WritelineEx(ftp_s, "250 created directory successfully\r\n", 1);
}

void cmd_RMD(char* request)
{
	char path[2048];
	strcpy(path, cwd);
	strcat(path, request+4);
	printf("remove dir %s = %d\n", path, rmdir(path));
	WritelineEx(ftp_s, "250 removed directory successfully\r\n", 1);
}

static char rename_from[2040];
void cmd_RNFR(char* request)
{
	char path[2048];
	strcpy(path, cwd);
	strcat(path, request+5);
	strcpy(rename_from, path);
	printf("DEBUG: rename from %s\n", rename_from);
	WritelineEx(ftp_s, "350 File Exists\r\n", 1);
}

void cmd_RNTO(char* request)
{
	char path[2048];
	strcpy(path, cwd);
	strcat(path, request+5);
	printf("DEBUG: rename to %s\n", path);
	lv2FsRename(rename_from, path);
	WritelineEx(ftp_s, "250 RNTO command successful\r\n", 1);
}
