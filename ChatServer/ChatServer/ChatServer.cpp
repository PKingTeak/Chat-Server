#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include<vector>
#include<mutex>
#include<thread>
#include<algorithm>
#pragma comment(lib, "ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS

static	std::vector<SOCKET> clients;
static	std::mutex clients_mutex;

void BroadCast(const std::string& msg, SOCKET sender)
{
	std::lock_guard<std::mutex> lock(clients_mutex);
	for (SOCKET client : clients)
	{
		if (client != sender)
		{

		send(client, msg.c_str(), (int)msg.size(), 0);
		}
	}
}

void Handle_client(SOCKET client_fd)
{
	char buf[1024];
	while (true)
	{
		int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
		if (n <= 0)
		{
			std::cout << "클라이언트 연결 종료";
			break;
		}
		buf[n] = '\0';
		std::string msg(buf);
		std::cout << "[수신]" << msg;


		BroadCast(msg, client_fd);
	}
	//연결 종료시 목록에서 제거
	{
		std::lock_guard<std::mutex> lock(clients_mutex);
		clients.erase(std::remove(clients.begin(), clients.end(), client_fd), clients.end());
	}
	closesocket(client_fd);
}

int main()

{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "WSAStartup 실패 \n";
		return 1;
	}
	//소캣 생성
	SOCKET serverfd = socket(AF_INET, SOCK_STREAM, 0); //IPv4 , TCP ;
	if (serverfd == INVALID_SOCKET)
	{
		std::cerr << "socket() 실패\n";
		WSACleanup();
		return 1;
	}

	

	//포트 재사용 옵션 (서버 껏다가 켰을때 already in use 방지)
	int opt = 1;
	setsockopt(serverfd, SOCK_STREAM, SO_REUSEADDR, (const char *) & opt, sizeof(opt));
	
	sockaddr_in addr{}; //해당 주소로 바인드
	addr.sin_family = AF_INET; 
	addr.sin_port = htons(9000); //포트 9000
	addr.sin_addr.s_addr = INADDR_ANY; //모든 IP에서 수신

	if (bind(serverfd, (sockaddr*)&addr, sizeof(addr)) < 0)
	{
		std::cerr << "bind() 실패";
		closesocket(serverfd);
		WSACleanup();
		return 1;
	}
	//연결 대기
	listen(serverfd, 10);
	std::cout << "서버 시작 , 포트 9000 대기 중..\n";
	//클라이언트 연결 수락(여기서 블로킹됨 - 클라이언트 올때 까지)
	
	//5. 에코루프
	char buf[1024];
	while (true)
	{
	sockaddr_in client_addr{};
	socklen_t client_len = sizeof(client_addr);
	SOCKET client_fd = accept(serverfd, (sockaddr*)&client_addr, &client_len);
		
	if (client_fd == INVALID_SOCKET)
	{
		continue;
	}
	std::cout << "클라이언트 접속";
	{
		//clients 목록에 추가 
		std::lock_guard<std::mutex> lock(clients_mutex);
		clients.push_back(client_fd);
	}
	// 새 스레드에 넘기고 메인 바로 다음 accept()
	std::thread(Handle_client, client_fd).detach();


	}


	//소캣 닫기
	closesocket(serverfd);
	WSACleanup();
	return 0;


}