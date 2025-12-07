#include <WinSock2.h>
#include <algorithm>
#include <cstring> 
#include <cstdint> 
#include <vector>
#include <cmath>
#include <iostream>
#include <unordered_map>

#pragma comment(lib, "ws2_32.lib")

#include "RingBuffer.h"
#include "PacketDefine.h"
#include "Session.h"
#include "CPacket.h"
#include "PacketStructs.h"

#include "CNetworkManager.h"
#include "PacketHelper.h"

// -------------------------------------------------------
// 좌표 관리용 구조체 및 배열
// -------------------------------------------------------
struct Point {
	short x;
	short y;
};

// 로그에 있는 좌표 21개를 미리 정의
Point g_spawnPoints[] = {
	{81, 207},  // ID 1
	{574, 320}, // ID 2
	{69, 344},  // ID 3
	{498, 298}, // ID 4
	{322, 444}, // ID 5
	{525, 165}, // ID 6
	{121, 367}, // ID 7
	{141, 231}, // ID 8
	{135, 162}, // ID 9
	{227, 136}, // ID 10
	{531, 304}, // ID 11
	{462, 253}, // ID 12
	{332, 242}, // ID 13
	{61, 456},  // ID 14
	{618, 195}, // ID 15
	{267, 226}, // ID 16
	{311, 118}, // ID 17
	{169, 212}, // ID 18
	{187, 119}, // ID 19
	{255, 274}, // ID 20
	{323, 151}  // ID 21 (MyCharacter)
};

// 현재 몇 번째 좌표를 사용할 차례인지 저장하는 인덱스
int g_spawnIndex = 0;

CNetworkManager::CNetworkManager()
	: m_listenSock(INVALID_SOCKET)
	, m_idGenerator(1)
	, m_handler(nullptr)
{
	FD_ZERO(&m_readSet);
}

CNetworkManager::~CNetworkManager()
{
	CleanUp();
}

bool CNetworkManager::Init(int port, INetworkHandler* handler)
{
	m_handler = handler;
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

	m_listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_listenSock == INVALID_SOCKET) return false;

	u_long on = 1;
	ioctlsocket(m_listenSock, FIONBIO, &on);

	SOCKADDR_IN serverAddr = {};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(port);

	if (bind(m_listenSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		return false;
	if (listen(m_listenSock, SOMAXCONN) == SOCKET_ERROR)
		return false;

	FD_ZERO(&m_readSet);
	FD_SET(m_listenSock, &m_readSet);

	m_handler = handler;

	std::cout << "Server Started on Port " << port << "...\n";
	return true;
}

void CNetworkManager::CleanUp()
{
	for (auto& pair : m_clients) {
		Session* session = pair.second;
		if (session) {
			closesocket(session->sock);
			delete session;
		}
	}
	m_clients.clear();
	if (m_listenSock != INVALID_SOCKET) {
		closesocket(m_listenSock);
		m_listenSock = INVALID_SOCKET;
	}
	WSACleanup();
}

void CNetworkManager::NetworkProc()
{
	FD_SET tempReadSet = m_readSet;
	FD_SET tempWriteSet;
	FD_ZERO(&tempWriteSet);

	// 보낼 데이터가 있는 세션 등록
	for (auto& pair : m_clients) {
		if (pair.second->sendBuffer.GetUseSize() > 0)
			FD_SET(pair.second->sock, &tempWriteSet);
	}

	TIMEVAL timeVal = { 0, 0 }; // Non-blocking 수준으로 빠르게 리턴
	int ret = select(0, &tempReadSet, &tempWriteSet, nullptr, &timeVal);
	if (ret == SOCKET_ERROR || ret == 0) return;
	
	// 1. Send 처리
	for (int i = 0; i < (int)tempWriteSet.fd_count; i++) {
		SOCKET sock = tempWriteSet.fd_array[i];
		if (m_clients.find(sock) != m_clients.end())
			FlushSendBuffer(m_clients[sock]);
	}
	
	// 2. Recv 및 Accept 처리
	for (int i = 0; i < (int)tempReadSet.fd_count; i++) {
		SOCKET sock = tempReadSet.fd_array[i];

		if (sock == m_listenSock) {
			AcceptNewClient();
		} else {
			if (m_clients.find(sock) == m_clients.end()) continue;
			Session* session = m_clients[sock];

			char tempRecv[2048];
			int recvLen = recv(sock, tempRecv, 2048, 0);

			if (recvLen <= 0) {
				if (recvLen == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) {
					// 데이터가 안 온 것뿐임 (대기)
				}
				else {
					// 연결 종료 or 에러
					Disconnect(sock);
				}
			}
			else {
				// 링버퍼에 데이터 적재
				session->recvBuffer.Enqueue(tempRecv, recvLen);

				// ★★★ 패킷 조립 및 핸들러 호출 루프 ★★★
				while (true) {
					// A. 헤더 사이즈 확인
					if (session->recvBuffer.GetUseSize() < sizeof(PACKET_HEADER)) break;

					// B. 헤더 엿보기 (Peek)
					PACKET_HEADER header;
					session->recvBuffer.Peek((char*)&header, sizeof(PACKET_HEADER));

					if (header.byCode != PACKET_CODE) {
						printf("[Error] Invalid Packet Code from User %d\n", session->id);
						Disconnect(sock);
						break;
					}

					// C. 전체 패킷 크기 확인 (헤더 + 바디)
					int totalSize = sizeof(PACKET_HEADER) + header.bySize;
					if (session->recvBuffer.GetUseSize() < totalSize) break;

					// D. 패킷 완성! 링버퍼에서 꺼내기
					char packetData[2048];
					session->recvBuffer.Dequeue(packetData, totalSize);

					// E. CPacket에 담기 (로직 전달용)
					CPacket packet;
					packet.PutData(packetData, totalSize);

					// F. 인터페이스(GameServer)로 전달
					if (m_handler) {
						m_handler->HandlePacket(session, packet);
					}
				}
			}
		}
	}
}

void CNetworkManager::AcceptNewClient()
{
	SOCKADDR_IN clientAddr;
	int addrLen = sizeof(clientAddr);
	SOCKET clientSock = accept(m_listenSock, (SOCKADDR*)&clientAddr, &addrLen);

	if (clientSock != INVALID_SOCKET) {
		// Nagle 알고리즘 해제 (반응속도 향상)
		BOOL optVal = TRUE;
		setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, (char*)&optVal, sizeof(optVal));

		// Non-blocking 설정
		u_long on = 1;
		ioctlsocket(clientSock, FIONBIO, &on);

		FD_SET(clientSock, &m_readSet);

		// 세션 생성
		Session* newSession = new Session();
		newSession->sock = clientSock;
		newSession->id = m_idGenerator++;
		newSession->direction = 3; // 기본 방향 (우측 등)
		newSession->hp = 100;
		newSession->isMoving = false;

		// ★★★ [이동된 로직] 좌표 배정 ★★★
		int maxPoints = sizeof(g_spawnPoints) / sizeof(Point);
		if (g_spawnIndex < maxPoints) {
			newSession->x = g_spawnPoints[g_spawnIndex].x;
			newSession->y = g_spawnPoints[g_spawnIndex].y;
		}
		else {
			// 랜덤 좌표 (예시)
			newSession->x = 50 + (rand() % 400);
			newSession->y = 50 + (rand() % 300);
		}
		g_spawnIndex++;

		// 맵에 등록
		m_clients[clientSock] = newSession;

		// ★★★ [이동된 로직] 접속 패킷 전송 ★★★
		// 1. 나한테 내 정보 보내기
		CPacket myPacket;
		PacketGenerator::MakeCreateMyCharacter(&myPacket, newSession->id, newSession->direction, newSession->x, newSession->y, newSession->hp);
		SendPacket(newSession, &myPacket);

		// 2. 나한테 다른 사람들 정보 보내기 & 다른 사람들에게 내 정보 보내기
		// (이 부분은 m_clients 순회해야 하므로 여기서 처리)
		CPacket otherPacket;
		PacketGenerator::MakeCreateOtherCharacter(&otherPacket, newSession->id, newSession->direction, newSession->x, newSession->y, newSession->hp);

		for (auto& pair : m_clients) {
			if (pair.second == newSession) continue;
			Session* oldUser = pair.second;

			// 기존 유저들에게 -> 신규 유저 정보 전송
			SendPacket(oldUser, &otherPacket);

			// 신규 유저에게 -> 기존 유저 정보 전송
			CPacket oldUserPacket;
			PacketGenerator::MakeCreateOtherCharacter(&oldUserPacket, oldUser->id, oldUser->direction, oldUser->x, oldUser->y, oldUser->hp);
			SendPacket(newSession, &oldUserPacket);
		}


	}
}

void CNetworkManager::Disconnect(SOCKET sock)
{
	if (m_clients.find(sock) != m_clients.end()) {
		Session* session = m_clients[sock];
		std::cout << "Client Disconnected: ID " << session->id << "\n";

		// ★★★ [추가] 중요: 떠나는 유저의 정보를 다른 사람들에게서 지워야 함 ★★★
		CPacket packet;
		PacketGenerator::MakeDeleteCharacter(&packet, session->id);

		// 나(죽은/나가는 사람)를 제외한 모든 사람에게 "쟤 지워라" 패킷 전송
		BroadcastPacket(session, &packet);

		// 소켓 정리 및 메모리 해제
		FD_CLR(sock, &m_readSet);
		closesocket(sock);
		delete session;
		m_clients.erase(sock);
	}
}

void CNetworkManager::SendPacket(Session* session, CPacket* packet)
{
	// CPacket -> char* 변환
	char* ptr = packet->GetBufferPtr();
	int size = packet->GetSize();

	// 송신 버퍼에 쌓기
	int enqSize = session->sendBuffer.Enqueue(ptr, size);

	if (enqSize < size) {
		// 버퍼 오버플로우 시 강퇴
		printf("[Error] SendBuffer Overflow User %d\n", session->id);
		Disconnect(session->sock);
	}
}

void CNetworkManager::BroadcastPacket(Session* excludeSession, CPacket* packet)
{
	for (auto& pair : m_clients) {
		if (pair.second == excludeSession) continue;
		SendPacket(pair.second, packet);
	}
}

void CNetworkManager::FlushSendBuffer(Session* session)
{
	if (session->sendBuffer.GetUseSize() <= 0) return;
	// 1. 버퍼 내용을 보지 않고 가져오기 (Peek)
	char tempBuf[10240];
	int peekSize = session->sendBuffer.Peek(tempBuf, session->sendBuffer.GetUseSize());
	// 2. 소켓 전송
	int sent = send(session->sock, tempBuf, peekSize, 0);
	if (sent == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK) {
			// 커널 버퍼가 꽉 참. 다음 턴에 다시 시도. 아무것도 안 하고 리턴.
			return;
		}
		else {
			// 진짜 에러
			Disconnect(session->sock);
		}
	}
	else if (sent > 0) {
		// 3. 보낸 만큼 링버퍼에서 제거
		session->sendBuffer.MoveFront(sent);
	}
}