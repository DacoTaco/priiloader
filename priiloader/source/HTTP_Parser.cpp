//HTTP Parser By DacoTaco
//Note by DacoTaco : yes this isn't the prettiest HTTP Parser alive but it does the job so stop complaining :)
#include "HTTP_Parser.h"
static char HTTP_Reply[4];
s32 GetHTTPFile(const char *host,const char *file,u8*& Data, int external_socket_to_use)
{
	//URL_REQUEST
	if(Data)
	{
		free_pointer(Data);
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
	sprintf( URL_Request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", file,host );
	if(external_socket_to_use == 0)
	{
		socket = ConnectSocket(host);
		if(socket < 0)
			return socket;
	}
	else
	{
		socket = external_socket_to_use;
	}
	//receive the header. it starts with something like "HTTP/1.1 [reply number like '200'] [name of code. 200 = 'OK']" { 48 54 54 50 2F 31 2E 31 20 xx xx xx 20 yy yy}
	bytes_send = net_send(socket, URL_Request, strlen(URL_Request), 0);
	if ( bytes_send > 0)
	{
		for( ;; )
		{
			bytes_read = 0;
			memset( buffer, '\0', 1024 );

			int i, n;
			for( i = 0;;)
			{
				n = net_recv(socket,(char*)&buffer[i],1, 0);

				if( n <= 0 )
				{
					gprintf("server closed connection\n");
					return -5;
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
					case 404: //File Not Found. nothing special we can do :)
					default: //o ow, a reply we dont understand yet D: close connection and bail out
						net_close(socket);
						return -6;
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
			gprintf("file size unknown!\n");
			if(!external_socket_to_use)
				net_close(socket);
			return -7;
		}
		else
		{
#ifdef _DEBUG
			gprintf("file is %d bytes\n",file_size);
#endif
		}
		Data = (u8*)memalign( 32, file_size );
		if (Data == NULL)
		{
			return -8;
		}
		memset(Data,0,sizeof(u8));
		s32 total = 0;
		gprintf( "Download: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\r",
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
				gprintf("\nconnection to server closed. error %d\n",bytes_read);
				return -9;
			}
			memcpy( &Data[total], buffer, bytes_read );
			total += bytes_read;
			int Ddone = (total *20 )/ file_size;
			gprintf("Download: ");
			while( Ddone )
			{
				gprintf( "%c", 178 );
				Ddone--;
			}
			gprintf( "\r" );
		}
		gprintf("\r\n");
	}
	else
	{
		gprintf("failed to send request packet to server! error %d",bytes_send);
		net_close(socket);
		free(Data);
		Data = NULL;
		return -4;
	}
	net_close(socket);
	return file_size;
}
s32 ConnectSocket(const char *hostname)
{
	s32 socket = 0;
	struct sockaddr_in connect_addr;
	if ( (socket = net_socket(AF_INET,SOCK_STREAM,IPPROTO_IP)) < 0)
	{
		sleep(1);
		return -1;
	}
	else
	{
		//gprintf("created socket!\nresolving %s to ip...\n",hostname);
		//resolving hostname
		struct hostent *host = 0;
		host = net_gethostbyname(hostname);
		if ( host == NULL )
		{
			//resolving failed
			gprintf("failed to resolve %s to ip.\n",hostname);
			sleep(1);
			return -2;
		}
		else
		{
			connect_addr.sin_family = AF_INET;
			connect_addr.sin_port = 80;
			memcpy(&connect_addr.sin_addr, host->h_addr_list[0], host->h_length);
		}
	}
	if (net_connect(socket, (struct sockaddr*)&connect_addr , sizeof(connect_addr)) == -1 )
	{
		/*gprintf("failed to connect to %s! something wrong in routing or is %s down?\nDNS server is up but could not connect"
				,inet_ntoa(connect_addr.sin_addr) , hostname );*/
		net_close(socket);
		sleep(1);
		return -3;
	}
	return socket;

}
const char* Get_Last_reply( void )
{
	return HTTP_Reply;
}