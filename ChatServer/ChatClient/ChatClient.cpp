#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <iostream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

SOCKET sock;

void receive_thread() {
    char buf[1024];
    while (true) {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            std::cout << "서버 연결 종료\n";
            break;
        }
        buf[n] = '\0';
        std::cout << "[상대방] " << buf;
    }
}

int main() {
    // 1. Winsock 초기화
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 2. 소켓 생성 및 서버 연결
    sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr))
        == SOCKET_ERROR) {
        std::cerr << "connect() 실패\n";
        return 1;
    }
    std::cout << "=== 서버 연결 성공! ===\n";

    // 3. 수신 전담 스레드 시작 (백그라운드)
    std::thread(receive_thread).detach();

    // 4. 메인은 송신만 담당
    std::string msg;
    while (true) {
        std::getline(std::cin, msg);
        if (msg == "quit") break;
        if (msg.empty()) continue;

        msg += "\n";
        send(sock, msg.c_str(), (int)msg.size(), 0);
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}