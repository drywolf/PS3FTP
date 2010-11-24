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

#define DATA_CON_BUF_LEN 32768 // 32kByte

//-----------------------------------------------------------------------
void cmd_USER()
{
	WritelineEx(ftp_s, "331 user accepted\r\n", 1);
}
//-----------------------------------------------------------------------
void cmd_PASS()
{
	WritelineEx(ftp_s, "230 password accepted\r\n", 1);
}
//-----------------------------------------------------------------------
void cmd_PWD()
{
	sprintf(temp, "257 \"%s\"\r\n", cwd);
	WritelineEx(ftp_s, temp, 1);
}
//-----------------------------------------------------------------------
void cmd_PASV()
{
	rest = 0;
	net_sockinfo_t snf;

	int ret = net_get_sockinfo(ftp_s, &snf, 1);

	if(ret >= 0 && snf.local_adr.s_addr != 0)
	{
		sprintf(temp, "227 Entering Passive Mode (%u,%u,%u,%u,240,206)\r\n", 
			(snf.local_adr.s_addr & 0xFF000000) >> 24, 
			(snf.local_adr.s_addr & 0xFF0000) >> 16, 
			(snf.local_adr.s_addr & 0xFF00) >> 8, 
			(snf.local_adr.s_addr & 0xFF));
	}
	else
	{
		sprintf(temp, "550 Error: can't fetch server IP address\r\n");
		WritelineEx(ftp_s, temp, 1);
		return;
	}

	WritelineEx(ftp_s, temp, 1);

	openSocket(61646, &data_s, &data_c, 1);
}
//-----------------------------------------------------------------------
void cmd_LIST()
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
			strcat(path, ent.d_name);

			struct stat entry; 
			stat(path, &entry);

			struct tm *tm;
			char timebuf[80];
			tm = localtime(&entry.st_mtime);
			strftime(timebuf, 80, "%b %d %Y", tm);

			sprintf(temp, "%srw-rw-rw-   1 root  root        %lu %s %s\r\n", 
				((entry.st_mode & S_IFDIR) != 0)?"d":"-", 
				entry.st_size, 
				timebuf, 
				ent.d_name);

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
}
//-----------------------------------------------------------------------
void cmd_RETR()
{
	WritelineEx(ftp_s, "150 Opening BINARY connection\r\n", 1);
	char filename[2048];

	absPath(filename, request+5);

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

	sprintf(temp, "%d %s ... %s\r\n", 
		(wr == rd)?226:426, 
		filename, 
		(wr == rd)?"Transfer Complete":"Transfer aborted");

	WritelineEx(ftp_s, temp, 1);
	closeSocket(data_c);
	closeSocket(data_s);
}
//-----------------------------------------------------------------------
void cmd_CWD()
{
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

	if(isDir(new_cwd))
	{
		sprintf(temp, "250 new pwd: %s\r\n", new_cwd);
		strcpy(cwd, new_cwd);
	}
	else
		sprintf(temp, "550 Could not change directory: %s\r\n", new_cwd);

	WritelineEx(ftp_s, temp, 1);
}
//-----------------------------------------------------------------------
void cmd_CDUP()
{
	strcpy(temp, "250 new pwd: ");

	for(int i=strlen(cwd)-2; i>0; i--)
	{
		if(cwd[i] != '/')
			cwd[i] = '\0';
		else
			break;
	}

	strcat(temp, cwd);
	strcat(temp, "\r\n");

	WritelineEx(ftp_s, temp, 1);
}
//-----------------------------------------------------------------------
void cmd_REST()
{
	rest = atoi(request+5);
	printf("rest set to %u\n", rest);
	WritelineEx(ftp_s, "200 rest set\r\n", 1);
}
//-----------------------------------------------------------------------
void cmd_DELE()
{
	absPath(temp, request+5);

	int ret = remove(temp);

	sprintf(temp, "%d %s\r\n", 
		(ret == 0)?250:550, 
		(ret == 0)?"File deleted":"Couldn't delete file");

	WritelineEx(ftp_s, temp, 1);
}
//-----------------------------------------------------------------------
void cmd_STOR()
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

	if(fd > 0)
	{
		while((rd = recv(data_s, buf, DATA_CON_BUF_LEN, MSG_WAITALL)) > 0)
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

	sprintf(temp, "%d %s ... %s\r\n", 
		(wr == rd)?226:426, 
		path, 
		(wr == rd)?"Transfer Complete":"Transfer aborted");

	WritelineEx(ftp_s, temp, 1);
	closeSocket(data_c);
	closeSocket(data_s);
}
//-----------------------------------------------------------------------
void cmd_MKD()
{
	absPath(temp, request+4);

	int ret = mkdir(temp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	sprintf(temp, "%d %s\r\n", 
		(ret == 0)?250:550, 
		(ret == 0)?"Created directory successfully":"Couldn't create directory");
	WritelineEx(ftp_s, temp, 1);
}
//-----------------------------------------------------------------------
void cmd_RMD()
{
	absPath(temp, request+4);

	int ret = -1;
	// if the target is no directory -> error
	if(isDir(temp))
		ret = rmdir(temp);

	sprintf(temp, "%d %s\r\n", 
		(ret == 0)?250:550, 
		(ret == 0)?"Deleted directory successfully":"Couldn't delete directory");

	WritelineEx(ftp_s, temp, 1);
}
//-----------------------------------------------------------------------
void cmd_RNFR()
{
	absPath(rename_from, request+5);

	// does source path exist ?
	int ret = exists(rename_from);

	sprintf(temp, "%d %s\r\n", 
		(ret == 0)?350:550, 
		(ret == 0)?"File exists":"File doesn't exist");

	WritelineEx(ftp_s, temp, 1);
}
//-----------------------------------------------------------------------
void cmd_RNTO()
{
	absPath(temp, request+5);

	// does target path already exist ?
	int ret = exists(temp);

	// only rename if target doesn't exist
	if(ret != 0)
		ret = lv2FsRename(rename_from, temp);

	sprintf(temp, "%d %s\r\n", 
		(ret == 0)?250:550, 
		(ret == 0)?"File renamed":"Target file already exists or renaming failed");

	WritelineEx(ftp_s, temp, 1);
}
//-----------------------------------------------------------------------
void cmd_SIZE()
{
	absPath(temp, request+5);

	// does path exist ?
	if(exists(temp) == 0)
	{
		struct stat entry; 
		stat(temp, &entry);

		sprintf(temp, "213 %lu\r\n", entry.st_size);
	}
	else
		sprintf(temp, "550 Requested file doesn't exist\r\n");

	WritelineEx(ftp_s, temp, 1);
}
//-----------------------------------------------------------------------
