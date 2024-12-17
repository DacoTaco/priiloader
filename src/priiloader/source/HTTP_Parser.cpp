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
static s16 lastStatusCode;
static s8 redirects = 0;

s16 GetLastHttpReply( void )
{
	return lastStatusCode;
}

s32 HttpGet(const char* host, const char* file, u8*& dataPtr, int* externalSocket)
{
	//reset last reply
	lastStatusCode = 0;

	//URL_REQUEST
	if (dataPtr)
		mem_free(dataPtr);

	//did we get a host of filename?
	if (host == NULL || file == NULL)
		return -4;

	const s32 bufferSize = 1024;
	char buffer[bufferSize];
	std::string location = "";
	u32 hostLocation;
	std::string newHost;
	std::string newFile;
	s32 ret = 0;
	s32 bytesTransfered = 0;
	u32 dataRead = 0;
	u32 dataSize = 0;
	int socket = 0;

	//setup the http request
	char httpRequest[bufferSize / 2] = { 0 };
	snprintf(httpRequest, bufferSize / 2 - 1, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Priiloader/%s(Nintendo Wii) DacoTacoIsGod/1.1 \r\n\r\n", file, host, GIT_REV_STR);

	//setup socket
	if (externalSocket == NULL || *externalSocket == 0)
	{
		socket = ConnectSocket(host, 80);
		if (socket < 0)
			return socket;
	}
	else
	{
		socket = *externalSocket;
	}

	//Set Get Request
	bytesTransfered = net_send(socket, httpRequest, strlen(httpRequest), 0);
	if (bytesTransfered <= 0)
	{
		ret = -5;
		goto _return;
	}

	//Read HTTP Headers
	for (s32 i = 0; i < bufferSize;)
	{
		bytesTransfered = net_recv(socket, &buffer[i], 1, 0);
		if (bytesTransfered <= 0 || (i + bytesTransfered) >= bufferSize)
		{
			ret = -6;
			goto _return;
		}

		//newline (0x0D0A) means we have a http header to parse
		if (i >= 1 && buffer[i - 1] == 0x0D && buffer[i] == 0x0A)
		{
			//headers are done. the rest is all data.
			if (i <= 1)
				break;

			//remove newline and parse header
			buffer[i] = 0;
			buffer[i - 1] = 0;

			std::string httpHeader = buffer;
			std::transform(httpHeader.begin(), httpHeader.end(), httpHeader.begin(), ::tolower);

			//HTTP reply header -> "HTTP/1.1 [reply number like '200'] [name of code. 200 = 'OK']" { 48 54 54 50 2F 31 2E 31 20 xx xx xx 20 yy yy}
			if (httpHeader.rfind("http/", 0) == 0)
			{
				s32 beginStatusCode = httpHeader.find(" ");
				s32 endStatusCode = httpHeader.find(" ", beginStatusCode + 1);

				if (beginStatusCode <= 0 || endStatusCode <= 0 || (endStatusCode - beginStatusCode) < 1 || endStatusCode - beginStatusCode > 4)
				{
					ret = -7;
					goto _return;
				}
				lastStatusCode = atoi(httpHeader.substr(beginStatusCode + 1, endStatusCode - beginStatusCode).c_str());
			}
			else if (httpHeader.rfind("content-length: ", 0) == 0)
			{
				dataSize = atoi(httpHeader.substr(16).c_str());
			}
			else if (httpHeader.rfind("localtion: ", 0) == 0)
			{
				//location
				location = httpHeader.substr(httpHeader.find("location: "));
			}

			i = 0;
			bytesTransfered = 0;
			memset(buffer, 0, bufferSize);
			continue;
		}

		i += bytesTransfered;
	}

	bytesTransfered = 0;
	memset(buffer, 0, bufferSize);

	//Check status code
	switch (lastStatusCode)
	{
	case 200: // OK
		if (redirects > 0)
		{
			redirects--;
			ret = dataSize;
			goto _return;
		}
		break;
	case 302:
	case 301: // Resource Moved
		if (location.size() == 0)
		{
			ret = -8;
			goto _return;
		}

		//we only support http links. no https or w/e, sorry
		if (location.rfind("http://", 0) != 0)
		{
			ret = -9;
			goto _return;
		}

		net_close(socket);
		socket = 0;

		hostLocation = location.find("/", 6);
		newFile = "";
		if (hostLocation == std::string::npos || hostLocation + 1 >= location.size())
		{
			newHost = location.substr(0, hostLocation);
		}
		else
		{
			int fileLocation = location.find("/", hostLocation + 1);
			newHost = location.substr(hostLocation + 1, fileLocation - hostLocation - 1);
			newFile = location.substr(fileLocation);
		}

		redirects++;
		dataSize = HttpGet(newHost.c_str(), newFile.c_str(), dataPtr, &socket);
		if (dataSize < 0)
		{
			ret = dataSize;
			goto _return;
		}

		//we were called from some redirect loop, so lets bail out
		if (redirects > 0)
			return dataSize;

		//we are no longer in a redirect loop. so we proceed like planned :)
		break;
	case 404: //File Not Found. nothing special we can do :)
	default: //o ow, a reply we dont understand yet D: close connection and bail out
		ret = -10;
		goto _return;
	}

	//http headers have been parsed. lets receive the actual data now.
	if (dataSize > 0)
	{
		dataPtr = (u8*)mem_malloc(dataSize);
		if (dataPtr == NULL)
		{
			ret = -11;
			goto _return;
		}
		memset(dataPtr, 0, dataSize);
	}

	ret = 0;
	while(1)
	{
		u32 len = bufferSize;
		void* netBuffer = buffer;
		if(dataSize > 0)
		{
			netBuffer = dataPtr + dataRead;
			len = dataSize - dataRead;
		}
		else
			memset(buffer, 0, bufferSize);

		bytesTransfered = net_recv(socket, netBuffer, len, 0);
		//connection closed or no more data
		if (bytesTransfered <= 0)
		{
			if (dataSize > 0)
				ret = -12;

			break;
		}

		DCFlushRange(netBuffer, bytesTransfered);
		DCInvalidateRange(netBuffer, bytesTransfered);

		//reallocate dataPtr as we read from it if we dont know the expected size
		if (dataSize <= 0)
		{
			if (dataPtr == NULL)
			{
				dataPtr = (u8*)mem_malloc(dataRead + bytesTransfered);
			}
			else
			{
				dataPtr = (u8*)mem_realloc(dataPtr, dataRead + bytesTransfered);
			}

			memcpy(dataPtr + dataRead, buffer, bytesTransfered);
			DCFlushRange(dataPtr + dataRead, bytesTransfered);
			DCInvalidateRange(dataPtr + dataRead, bytesTransfered);
		}

		dataRead += bytesTransfered;
		if (dataSize > 0 && dataRead >= dataSize)
			break;
	}

	if(ret >= 0)
		ret = dataRead;

_return:
	if (ret <= 0 && dataPtr != NULL)
		mem_free(dataPtr);	

	if (externalSocket != NULL && socket > 0)
	{
		net_close(socket);
		socket = 0;
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