#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <fcntl.h>
#include <fcntl.h>
#include <sys/poll.h>  

// 从原来的main.cpp里面copy的
#include "cryptoTools/Crypto/RandomOracle.h"
#include "cryptoTools/Common/MatrixView.h"
#include "libOTe/TwoChooseOne/KosOtExtReceiver.h"
#include "libOTe/TwoChooseOne/KosOtExtSender.h"
#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Common/Timer.h"
#include "cryptoTools/Network/Channel.h"
#include "cryptoTools/Network/Endpoint.h"
#include "cryptoTools/Network/IOService.h"
#include "cryptoTools/Common/Defines.h"
//引入自己写的
#include "TPsiMain.h"

void clientSend(char* data, int port) {
	int sockClient = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addrSrv;
	addrSrv.sin_addr.s_addr = inet_addr("127.0.0.1");
	addrSrv.sin_family = AF_INET;
	addrSrv.sin_port = htons(port);
	connect(sockClient, (const struct sockaddr*)&addrSrv, sizeof(struct sockaddr_in));
	//char szSendBuf[100] = "this is test";
	//send(sockClient, szSendBuf, strlen(szSendBuf) + 1, 0);
	send(sockClient, data, strlen(data) + 1, 0);
	close(sockClient);
}

void serverRecv(int port) {
	int BACKLOG = 100;
	int index = 0; // 记录当前通信的数量
	int iListenSock = socket(AF_INET, SOCK_STREAM, 0);
	int arrayConnSock[BACKLOG] = { 0 }; // 这个初始化并不好，但其实无大碍
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	inet_aton("0.0.0.0", &addr.sin_addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	int iOpt = 1;
	setsockopt(iListenSock, SOL_SOCKET, SO_REUSEADDR, &iOpt, sizeof(iOpt));  // 标配
	bind(iListenSock, (sockaddr*)&addr, sizeof(addr));
	listen(iListenSock, 100);

	int flag = -1;
	pollfd fds[BACKLOG + 1];        // BACKLOG个通信socket和1个监听socket
	memset(fds, flag, sizeof(fds)); // 设置一个标志
	fds[0].fd = iListenSock;        // 监听socket
	fds[0].events = POLLIN;

	int ndfs = 1;
	int timeoutMS = -1; // 永不超时
	int i = 0;
	for (int i = 1; i <= BACKLOG; i++)
	{
		int iRet = poll(fds, ndfs, timeoutMS);
		if (iRet < 0)
		{
			printf("poll error, iRet %d\n", iRet);
			continue;
		}

		if (iRet < 0)
		{
			printf("poll error, iRet %d\n", iRet);
			continue;
		}

		if (fds[0].revents & POLLIN) // fds[0]对应iListenSock， 此处表明检测到有新客户端连接过来
		{
			arrayConnSock[i - 1] = accept(iListenSock, NULL, NULL); // arrayConnSock是通信socket
			fds[i].fd = arrayConnSock[i - 1];
			fds[i].events = POLLIN;
			ndfs++;
			printf("new client came, local fd is [%d]\n", arrayConnSock[i - 1]);
		}

		for (int j = 0; j < BACKLOG; j++)
		{
			char szBuf[1024] = { 0 };
			if (arrayConnSock[j] > 0 && (fds[j + 1].revents & POLLIN)) // 此处检测的是已经链接的socket的读事件
			{
				int recvLen = recv(arrayConnSock[j], szBuf, sizeof(szBuf) - 1, 0);
				if (recvLen > 0)
				{
					printf("recv data [%s] from local fd [%d] \n", szBuf, arrayConnSock[j]);
					index += 1;
					if (index == 2)
					{
						j = BACKLOG + 1;
						i = BACKLOG + 1;
						break;
					}

				}
				else if (0 == recvLen)
				{
					close(arrayConnSock[j]);
					memset(&fds[j + 1], flag, sizeof(pollfd));
					printf("connection closed fd [%d]\n", arrayConnSock[j]);
					arrayConnSock[j] = 0;
				}
				else
				{
					close(arrayConnSock[j]);
					memset(&fds[j + 1], flag, sizeof(pollfd));
					printf("recv error, recvLen is %d", recvLen);
					arrayConnSock[j] = 0;
				}
			}
		}

	}

	close(iListenSock);
}

int main(int argc, char** argv) {
	// argv[1] = 参与方的角色,{0,1,...,n}
	// argv[2] = 参与方的数量 n
	if (argv[1][0] == '0')
	{

		// 参与方P1
		TPsiRecv(atoi(argv[2]), atoi(argv[3]));


	}
	else if (argv[1][0] == '1')
	{
		// 参与方P0
		TPsiSend(1, atoi(argv[2]), atoi(argv[3]));
	}
	else
	{
		// 其他参与方
		TPsiSend(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));

	}



	return 0;
}
