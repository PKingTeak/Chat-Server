#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include<vector>
#include<mutex>
#include<thread>
#include<queue>
#include<condition_variable>
#include<algorithm>
#pragma comment(lib, "ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS

struct Client
{
	SOCKET socket;
	std::string name;


};

static	std::vector<Client> clients;
static	std::mutex clients_mutex;
static std::condition_variable queue_cv; //일이 없을때 스레드 재우기 용으로 CPU 낭비 방지
static std::mutex queue_mutex;
static std::queue<std::string> message_queue;



void broadcast_worker()
{
	while (true)
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		queue_cv.wait(lock, [] {return !message_queue.empty(); }); //메세지가 없으면 대기 

		std::string msg = message_queue.front();
		message_queue.pop();
		lock.unlock();
		for (auto& c : clients)
		{
			send(c.socket, msg.c_str(), (int)msg.size(), 0);
		}
	}
}

void enqueue_message(const std::string& msg)
{
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		message_queue.push(msg);
	}
	queue_cv.notify_one();// 해당 스레드 깨우기. 메세지 전달해야하니까 
}



void Handle_client(SOCKET client_fd)
{
	char buf[1024];
	int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
	buf[n] = '\0';
	std::string name(buf);



	{
		std::lock_guard<std::mutex> lock(clients_mutex);
		for (auto& c : clients)
		{
			if (c.socket == client_fd)
			{
				c.name = name;
			}
		}
	}
	while (true)
	{


		int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
		if (n <= 0)
		{
			std::cout << "클라이언트 연결 종료";
			break;
		}
		buf[n] = '\0';
		std::string msg = name + ": " + buf;
		enqueue_message(msg);

		

	
	}
	//연결 종료시 목록에서 제거
	{
		std::lock_guard<std::mutex> lock(clients_mutex);
		clients.erase(std::remove_if(clients.begin(), clients.end(),
			[client_fd](const Client& c) { return c.socket == client_fd; }),
			clients.end());
	}
	closesocket(client_fd);
}

void broadcast_thread(std::string msg)
{
	{
		std::lock_guard<std::mutex> lock(queue_mutex);
		message_queue.push(msg);
	}

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
	setsockopt(serverfd, SOCK_STREAM, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

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
	
	std::thread(broadcast_worker).detach(); //연결 전에 브로드캐스트 스레드 먼저 띄워서 대기 시키기
	

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
			clients.push_back({ client_fd ,""});
		}
		// 새 스레드에 넘기고 메인 바로 다음 accept()
		std::thread(Handle_client, client_fd).detach();


	}


	//소캣 닫기
	closesocket(serverfd);
	WSACleanup();
	return 0;


}