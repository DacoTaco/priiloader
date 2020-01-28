/*

HTTP Parser by DacoTaco
Copyright (C) 2013-2019  DacoTaco

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
#include <algorithm>
#include <cctype>
#include <string>
#include "HTTP_Parser.h"
#include "../../Shared/gitrev.h"
static char HTTP_Reply[4];
static s8 redirects = 0;
s32 GetHTTPFile(const char *host,const char *file,u8*& Data, int* external_socket_to_use)
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
	const int buffer_size = 1024;
	char buffer[buffer_size];
	s32 bytes_read = 0;
	s32 bytes_send = 0;
	int file_size = 0;
	int socket = 0;
	s32 ret = 0;
	char URL_Request[buffer_size / 2];
	//example : "GET /daco/version.dat HTTP/1.0\r\nHost: www.dacotaco.com\r\nUser-Agent: DacoTacoIsGod/version\r\n\r\n\0"
	sprintf( URL_Request, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Priiloader/%s(Nintendo Wii) DacoTacoIsGod/1.0 \r\n\r\n", file,host,GIT_REV_STR );
	if(external_socket_to_use == 0 || *external_socket_to_use == 0)
	{
		socket = ConnectSocket(host,80);
		if(socket < 0)
			return socket;
	}
	else
	{
		socket = *external_socket_to_use;
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
					//it is possible we get here cause there is no content_lenght mentioned. which fails
					//that can be fixed by reading anyway and reallocating as we go. not gonna implement that atm
					ret = -6;
					goto return_ret;
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
				std::string location;
				char* phost = NULL;
				char host_new[buffer_size];
				char file_new[buffer_size];
				//process the HTTP reply. full list @ http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
				switch(atoi(HTTP_Reply))
				{
					case 200: // 200 - OK . everything is fine. lets proceed
						if(redirects > 0)
						{
							redirects--;
							ret = 1;
							goto return_ret;
						}
						break;
					case 302:
					case 301: //moved. to lazy to implement that :P
						{
							//first lets get the new link.
							//HTTP/1.1 301 Moved Permanently..Location: http://www.google.com/..Content-Type: 
							memset( buffer, '\0', buffer_size );
							for( i = 0;i < 265;)
							{
								n = net_recv(socket,(char*)&buffer[i],1, 0);

								if( n <= 0 )
								{
									ret = -6;
									goto return_ret;
								}

								i += n;

								if( i > 1 )
								{
									if( buffer[i-1] == 0x0A && buffer[i-2] == 0x0D )
										break;
								}
							}
							std::string header(buffer);
							std::transform(header.begin(), header.end(), header.begin(), ::tolower);							
							if(header.compare(0,strlen("location: "),"location: "))
							{
								location = header.substr(	header.find("location: "),
																		header.size() - header.find("location: "));
								int start = strlen("location: ");
								int end = location.npos;
								if(location.compare(0,2,"\r\n"))
									end = location.find("\r\n") - start;

								location = location.substr(start,end);
							}
							else
							{
								//the location wasn't there, or the server wasn't following protocol. so lets bail out
								net_close(socket);
								ret = -88;
								goto return_ret;
							}
							//we got the url. close everything and start all over
							net_close(socket);
							memset( buffer, '\0', buffer_size );
							bytes_send = 0;
							bytes_read = 0;
							memset(host_new,0,buffer_size);
							memset(file_new,0,buffer_size);
							//extract host & file from url
							if( location.compare(0,7,"http://"))
							{
								strncpy(buffer,location.c_str(),buffer_size-1);
							}
							else
							{
								//no http:// found so its prob not a valid link. bail out.
								ret = -99;
								goto return_ret;
							}
							phost = strchr(buffer,'/');
							//note : use snprintf instead of strncpy when im done and make the host/file none-const and make the code more permanent
							if(phost != NULL)
							{
								strncpy(host_new,buffer,phost-buffer);
								memcpy(file_new,&buffer[phost-buffer],buffer_size-strnlen(host_new,buffer_size));
							}
							else
							{
								strncpy(file_new,"/\0",2);
								memcpy(host_new,buffer,strlen(buffer));
							}
							gprintf("new host & file : %s & %s",host_new,file_new);
							redirects++;
							GetHTTPFile(host_new,file_new,Data,&socket);
							if(redirects > 0)
							{
								//we were called from some redirect loop, so lets bail out
								return 1;
							}
							else
							{
								//we are no longer in a redirect loop. so we proceed like planned :)
								break;
							}
						}
						break;
					case 404: //File Not Found. nothing special we can do :)
					default: //o ow, a reply we dont understand yet D: close connection and bail out
						net_close(socket);
						ret = -7;
						goto return_ret;
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
			ret = -8;
			goto return_ret;
		}
		Data = (u8*)mem_malloc( file_size );
		if (Data == NULL)
		{
			ret = -9;
			goto return_ret;
		}
		memset(Data,0,file_size);
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
				ret = -10;
				goto return_ret;
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
		ret = -5;
		goto return_ret;
	}
	net_close(socket);
	return file_size;
return_ret:
	if(ret < 0)
	{
		net_close(socket);
		redirects = 0;
	}
	if(external_socket_to_use != 0)
	{
		memset(external_socket_to_use,socket,sizeof(int));
	}
	return ret;
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