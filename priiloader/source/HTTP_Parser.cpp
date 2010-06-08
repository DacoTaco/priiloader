//HTTP Parser By DacoTaco
//Note by DacoTaco : yes this isn't the prettiest HTTP Parser alive but it does the job so stop complaining :)
#include "HTTP_Parser.h"
s32 GetHTTPFile(int socket, const char *host,const char *file,u8*& Data)
{
	//URL_REQUEST
	if(Data)
	{
		free_pointer(Data);
	}
	char buffer[1024];
	s32 bytes_read = 0;
	s32 bytes_send = 0;
	s32 file_size = 0;
	char URL_Request[512];
	sprintf( URL_Request, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", file,host );
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
					return -1;
				}

				i += n;

				if( i > 1 )
				{
					if( buffer[i-1] == 0x0A && buffer[i-2] == 0x0D )
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
			net_close(socket);
			return -2;
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
			gprintf("failed to mem align memory for update\n");
			return -3;
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
				net_close(socket);
				gprintf("\nconnection to server closed. error %d\n",bytes_read);
				return -4;
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
		return -1;
	}
	net_close(socket);
	return file_size;
}
u32 ConnectSocket(const char *hostname)
{
	u32 socket = 0;
	struct sockaddr_in connect_addr;
	if ( (socket = net_socket(AF_INET,SOCK_STREAM,IPPROTO_IP)) < 0)
	{
		net_close(socket);
		gprintf("error creating socket");
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
	//gprintf("connecting to %s(%s)...\n",hostname,inet_ntoa(connect_addr.sin_addr));
	if (net_connect(socket, (struct sockaddr*)&connect_addr , sizeof(connect_addr)) == -1 )
	{
		gprintf("failed to connect to %s! something wrong in routing or is %s down?\nDNS server is up but could not connect"
				,inet_ntoa(connect_addr.sin_addr) , hostname );
		net_close(socket);
		sleep(1);
		return -3;
	}
	return socket;

}