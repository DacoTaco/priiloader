/*

HTTP Parser by DacoTaco
Copyright (C) 2009-20010  DacoTaco

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


*/
//Note by DacoTaco : yes this isn't the prettiest HTTP Parser alive but it does the job better then loads of http parsers so stop complaining :)
#include "HTTP_Parser.h"
#include "../../Shared/svnrev.h"
static char HTTP_Reply[4];
s32 GetHTTPFile(const char *host,const char *file,u8*& Data, int external_socket_to_use)
{
	//URL_REQUEST
	if(Data)
	{
		mem_free(Data);
	}
	if(host == NULL || file == NULL)
	{
		//blaarg, we didn't get host or file. fail!
		return -4;
	}
	if(HTTP_Reply[0] != 0)
	{
		//reset last reply
		memset(HTTP_Reply,0,4);
	}
	char buffer[1024];
	s32 bytes_read = 0;
	s32 bytes_send = 0;
	s32 file_size = 0;
	int socket;
	char URL_Request[512];
	//example : "GET /daco/version.dat HTTP/1.0\r\nHost: www.dacotaco.com\r\nUser-Agent: DacoTacoIsGod/version\r\n\r\n\0"
	sprintf( URL_Request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Priiloader/%s(Nintendo Wii) DacoTacoIsGod/1.0 \r\n\r\n", file,host,SVN_REV_STR );
	if(external_socket_to_use == 0)
	{
		socket = ConnectSocket(host,80);
		if(socket < 0)
			return socket;
	}
	else
	{
		socket = external_socket_to_use;
	}
	bytes_send = net_send(socket, URL_Request, strlen(URL_Request), 0);
	if ( bytes_send > 0)
	{
		//receive the header. it starts with something like "HTTP/1.1 [reply number like '200'] [name of code. 200 = 'OK']" { 48 54 54 50 2F 31 2E 31 20 xx xx xx 20 yy yy}
		for( ;; )
		{
			bytes_read = 0;
			memset( buffer, '\0', 1024 );

			int i, n;
			for( i = 0;i < 265;)
			{
				n = net_recv(socket,(char*)&buffer[i],1, 0);

				if( n <= 0 )
				{
					return -6;
				}

				i += n;

				if( i > 1 )
				{
					if( buffer[i-1] == 0x0A && buffer[i-2] == 0x0D )
						break;
				}
			}
			if(HTTP_Reply[0] == 0)
			{
				strncpy(HTTP_Reply,&buffer[9],3);
				HTTP_Reply[3] = '\0';
				//process the HTTP reply. full list @ http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
				switch(atoi(HTTP_Reply))
				{
					case 200: // 200 - OK . everything is fine. lets proceed
						break;
					case 302: //moved. to lazy to implement that :P
					case 404: //File Not Found. nothing special we can do :)
					default: //o ow, a reply we dont understand yet D: close connection and bail out
						net_close(socket);
						return -7;
						break;
				}
			}	
			if( !memcmp( buffer, "Content-Length: ", 16 ) )
			{
				sscanf( buffer , "Content-Length: %d", &file_size );
			}
			else if( !memcmp( buffer, "\x0D\x0A", 2 ) )
				break;
		}
		if(file_size == 0)
		{
			if(!external_socket_to_use)
				net_close(socket);
			return -8;
		}
		Data = (u8*)mem_malloc( file_size );
		if (Data == NULL)
		{
			return -9;
		}
		memset(Data,0,sizeof(u8));
		s32 total = 0;
		gdprintf( "Download: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\r",
		176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176 );
		while (total != file_size)
		{
			bytes_read = 0;
			bytes_read = net_recv(socket,buffer,1024, 0);
			if(!bytes_read)
			{
				if(!external_socket_to_use)
					net_close(socket);
				free(Data);
				Data = NULL;
				return -10;
			}
			memcpy( &Data[total], buffer, bytes_read );
			total += bytes_read;
			int Ddone = (total *20 )/ file_size;
			gdprintf("Download: ");
			while( Ddone )
			{
				gdprintf( "%c", 178 );
				Ddone--;
			}
			gdprintf( "\r" );
		}
		gdprintf("\r\n");
	}
	else
	{
		net_close(socket);
		free(Data);
		Data = NULL;
		return -5;
	}
	net_close(socket);
	return file_size;
}
s32 ConnectSocket(const char *hostname, u32 port)
{
	s32 socket = 0;
	struct sockaddr_in connect_addr;
	if ( (socket = net_socket(AF_INET,SOCK_STREAM,IPPROTO_IP)) < 0)
	{
		return -1;
	}
	else
	{
		//resolving hostname
		struct hostent *host = 0;
		host = net_gethostbyname(hostname);
		if ( host == NULL )
		{
			//resolving failed
			return -2;
		}
		else
		{
			connect_addr.sin_family = AF_INET;
			connect_addr.sin_port = port;
			memcpy(&connect_addr.sin_addr, host->h_addr_list[0], host->h_length);
		}
	}
	if (net_connect(socket, (struct sockaddr*)&connect_addr , sizeof(connect_addr)) == -1 )
	{
		net_close(socket);
		return -3;
	}
	return socket;

}
const char* Get_Last_reply( void )
{
	return HTTP_Reply;
}