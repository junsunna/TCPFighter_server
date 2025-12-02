#include <iostream>
#include <vector>
#include <WinSock2.h>
#include <Windows.h>
#include <unordered_map>
#include <cstdlib>
#include <ctime>
#include <cmath>

#include "PacketDefine.h"
#include "PacketStructs.h"
#include "RingBuffer.h"
#include "PacketHelper.h"

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 5000
#define PACKET_CODE 0x89
#define GAME_FPS    50

// 공격 쿨타임
#define dfDELAY_ATTACK1  200   // 0.5초 (공격 1, 2는 동일)
#define dfDELAY_ATTACK2  200   // 0.5초
#define dfDELAY_ATTACK3  400  // 1.0초

// -------------------------------------------------------
// [설정] 게임 상수 정의
// -------------------------------------------------------
// 화면 이동 영역
#define dfRANGE_MOVE_TOP    50
#define dfRANGE_MOVE_LEFT   10
#define dfRANGE_MOVE_RIGHT  630
#define dfRANGE_MOVE_BOTTOM 470

// 이동 속도
#define dfSPEED_X           3
#define dfSPEED_Y           2

// 오차 체크 범위 (50픽셀 이상 차이나면 퇴장)
#define dfERROR_RANGE       50

#define dfDAMAGE1			10
#define dfDAMAGE2			20
#define dfDAMAGE3			30
#define dfATTACK1_RANGE_X 80
#define dfATTACK1_RANGE_Y 10

#define dfATTACK2_RANGE_X 90
#define dfATTACK2_RANGE_Y 10

#define dfATTACK3_RANGE_X 100
#define dfATTACK3_RANGE_Y 20
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
// -------------------------------------------------------
// 세션 구조체
// -------------------------------------------------------
struct Session {
	SOCKET sock;
	int id;

	short x, y;
	unsigned char direction;
	unsigned char hp;

	bool isMoving;

	// ★ 송신용 링버퍼 추가 ★
	RingBuffer recvBuffer;
	RingBuffer sendBuffer;
	ULONGLONG nextActionEnableTime;
	Session() : sock(INVALID_SOCKET), id(-1), x(0), y(0), direction(0), hp(100), isMoving(false) {
		recvBuffer.Resize(10240);
		sendBuffer.Resize(10240);
		nextActionEnableTime = 0;
	}
};

std::unordered_map<SOCKET, Session*> g_clients;
int g_idGenerator = 1;
// -------------------------------------------------------
// 네트워크/로직 관련 전역 변수 (Select 모델)
// -------------------------------------------------------
SOCKET g_listenSock;
FD_SET g_readSet;
FD_SET g_writeSet;

// -------------------------------------------------------
// 함수 선언
// -------------------------------------------------------
void NetworkProc(); // 네트워크 처리 (Select)
void UpdateGame();  // 게임 로직 (50 FPS)
void MoveCharacter(Session* session); // 캐릭터 이동 계산
bool CheckBoundary(short x, short y); // 좌표 유효성 검사

void SendPacket(Session* session, char* packet, int size);
void BroadcastPacket(Session* excludeSession, char* packet, int size);
void Disconnect(SOCKET sock);
void HandlePacket(Session* session, char* packet);
void FlushSendBuffer(Session* session); // ★ 실제 전송을 담당할 함수

void ProcAttack(Session* attacker, int attackType);
// 행동 가능 여부 체크 및 딜레이 적용 함수
bool CheckGlobalCooldown(Session* session, int attackType);
// -------------------------------------------------------
// 메인 함수
// -------------------------------------------------------
int main() {
	srand((unsigned int)time(NULL));
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 0;

	g_listenSock = socket(AF_INET, SOCK_STREAM, 0);

	u_long on = 1;
	ioctlsocket(g_listenSock, FIONBIO, &on);

	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(SERVER_PORT);

	bind(g_listenSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	listen(g_listenSock, SOMAXCONN);

	printf("Server Started on Port %d...\n", SERVER_PORT);

	FD_SET readSet;  // 읽기 감시 셋 (Master)
	FD_SET writeSet; // 쓰기 감시 셋 (매 루프마다 초기화)

	FD_ZERO(&readSet);
	FD_SET(g_listenSock, &g_readSet);

	// -------------------------------------------------------
	// [QueryPerformanceCounter] 50 FPS 루프 설정
	// -------------------------------------------------------
	LARGE_INTEGER frequency;
	LARGE_INTEGER prevTime, currTime;

	QueryPerformanceFrequency(&frequency); // 타이머 주파수 가져오기
	QueryPerformanceCounter(&prevTime);    // 시작 시간 측정

	// 1초를 주파수로 나눈 값 (시간 계산용 스케일)
	double timeScale = 1.0 / frequency.QuadPart;
	double accumulatedTime = 0.0;
	const double targetFrameTime = 1.0 / GAME_FPS; // 0.02초 (20ms)


	// 메인 루프
	while (true) {
		// 현재 시간 측정
		QueryPerformanceCounter(&currTime);
		accumulatedTime += (currTime.QuadPart - prevTime.QuadPart) * timeScale;
		prevTime = currTime;

		// 네트워크 처리
		NetworkProc();

		// 누적된 시간이 1프레임(0.02초)을 넘어가면 게임 로직 실행 (Catch up)
		while (accumulatedTime >= targetFrameTime) {
			UpdateGame(); // ★ 50 FPS 보장 로직
			accumulatedTime -= targetFrameTime;
		}

	}

	WSACleanup();
	return 0;
}

// 행동 가능 여부 체크 및 딜레이 적용 함수
bool CheckGlobalCooldown(Session* session, int attackType) {
	// 1. 현재 시간
	ULONGLONG currentTick = GetTickCount64();

	// 2. [검사] 아직 행동 불가 시간인가?
	if (currentTick < session->nextActionEnableTime) {
		// 남은 시간 계산 (로그용)
		// ULONGLONG remain = session->nextActionEnableTime - currentTick;
		// printf("[GCD] User %d is busy. Wait %llu ms.\n", session->id, remain);
		return false;
	}

	// 3. [적용] 스킬별로 다음 행동 가능한 시간을 다르게 설정
	DWORD delay = 0;
	switch (attackType) {
	case 1: delay = dfDELAY_ATTACK1; break; // 500ms
	case 2: delay = dfDELAY_ATTACK2; break; // 500ms
	case 3: delay = dfDELAY_ATTACK3; break; // 1000ms
	default: return false;
	}

	// "현재 시간 + 딜레이"까지 행동 불가로 설정
	session->nextActionEnableTime = currentTick + delay;

	return true;
}

// -------------------------------------------------------
// [게임 로직] 50 FPS마다 호출됨
// -------------------------------------------------------
void UpdateGame() {
	// 모든 유저를 순회하며 이동 중인 유저의 좌표 업데이트
	auto iter = g_clients.begin();
	while (iter != g_clients.end()) {
		Session* session = iter->second;

		if (session->isMoving) {
			MoveCharacter(session);
		}

		iter++;
	}
}

// -------------------------------------------------------
// [이동 로직] 서버 시뮬레이션 & 벽 충돌 처리
// -------------------------------------------------------
void MoveCharacter(Session* session) {
	short nextX = session->x;
	short nextY = session->y;

	// 8방향 이동 계산
	switch (session->direction) {
	case dfPACKET_MOVE_DIR_LL: nextX -= dfSPEED_X; break;
	case dfPACKET_MOVE_DIR_LU: nextX -= dfSPEED_X; nextY -= dfSPEED_Y; break;
	case dfPACKET_MOVE_DIR_UU: nextY -= dfSPEED_Y; break;
	case dfPACKET_MOVE_DIR_RU: nextX += dfSPEED_X; nextY -= dfSPEED_Y; break;
	case dfPACKET_MOVE_DIR_RR: nextX += dfSPEED_X; break;
	case dfPACKET_MOVE_DIR_RD: nextX += dfSPEED_X; nextY += dfSPEED_Y; break;
	case dfPACKET_MOVE_DIR_DD: nextY += dfSPEED_Y; break;
	case dfPACKET_MOVE_DIR_LD: nextX -= dfSPEED_X; nextY += dfSPEED_Y; break;
	}

	// ★ 벽 충돌 처리 (중요!)
	// "상단으로 벽을 타고 이동하는게 아니며, 그자리에 멈춰야 함"
	// -> 범위를 벗어나면 아예 업데이트를 하지 않음 (이전 좌표 유지)
	if (CheckBoundary(nextX, nextY)) {
		session->x = nextX;
		session->y = nextY;
	}
	// else: 멈춤 (MoveCharacter에서 아무것도 안 함)
}

// -------------------------------------------------------
// [범위 체크] 화면 밖으로 나가는지 확인
// -------------------------------------------------------
bool CheckBoundary(short x, short y) {
	if (x < dfRANGE_MOVE_LEFT || x > dfRANGE_MOVE_RIGHT) return false;
	if (y < dfRANGE_MOVE_TOP || y > dfRANGE_MOVE_BOTTOM) return false;
	return true; // 이동 가능
}

// -------------------------------------------------------
// [네트워크 처리] Select 모델
// -------------------------------------------------------
void NetworkProc() {
	FD_SET tempReadSet = g_readSet;
	FD_SET tempWriteSet;
	FD_ZERO(&tempWriteSet);

	// 보낼 데이터가 있는 세션 등록
	for (auto& pair : g_clients) {
		if (pair.second->sendBuffer.GetUseSize() > 0)
			FD_SET(pair.second->sock, &tempWriteSet);
	}

	TIMEVAL timeVal = { 0, 0 }; // Non-blocking 수준으로 빠르게 리턴

	int ret = select(0, &tempReadSet, &tempWriteSet, nullptr, &timeVal);
	if (ret == SOCKET_ERROR || ret == 0) return;

	// 1. Send 처리
	for (int i = 0; i < (int)tempWriteSet.fd_count; i++) {
		SOCKET sock = tempWriteSet.fd_array[i];
		if (g_clients.find(sock) != g_clients.end())
			FlushSendBuffer(g_clients[sock]);
	}

	// 2. Recv 및 Accept 처리
	for (int i = 0; i < (int)tempReadSet.fd_count; i++) {
		SOCKET sock = tempReadSet.fd_array[i];

		if (sock == g_listenSock) {
			// [Accept]
			SOCKADDR_IN clientAddr;
			int addrLen = sizeof(clientAddr);
			SOCKET clientSock = accept(g_listenSock, (SOCKADDR*)&clientAddr, &addrLen);

			if (clientSock != INVALID_SOCKET) {
				BOOL optVal = TRUE;
				setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, (char*)&optVal, sizeof(optVal));
				printf("[Connect] Client: %d\n", clientSock);
				u_long on = 1;
				ioctlsocket(clientSock, FIONBIO, &on);
				FD_SET(clientSock, &g_readSet);

				Session* newSession = new Session();
				newSession->sock = clientSock;
				newSession->id = g_idGenerator++;
				newSession->direction = dfPACKET_MOVE_DIR_RR; // 기본 방향
				newSession->isMoving = false; // 정지 상태

				// 스폰 좌표 배정
				int maxPoints = sizeof(g_spawnPoints) / sizeof(Point);
				if (g_spawnIndex < maxPoints) {
					// 1. 배열에 있는 좌표 사용 (0 ~ 20번 인덱스)
					newSession->x = g_spawnPoints[g_spawnIndex].x;
					newSession->y = g_spawnPoints[g_spawnIndex].y;
				}
				else {
					// 2. 21번째 유저부터는 랜덤 좌표 생성
					// 이동 가능 범위 내에서 안전하게 생성 (벽에 너무 붙지 않게 +20 여유)
					// 범위: Left(10) ~ Right(630), Top(50) ~ Bottom(470)

					int safeWidth = (dfRANGE_MOVE_RIGHT - 20) - (dfRANGE_MOVE_LEFT + 20);
					int safeHeight = (dfRANGE_MOVE_BOTTOM - 20) - (dfRANGE_MOVE_TOP + 20);

					newSession->x = (dfRANGE_MOVE_LEFT + 20) + (rand() % safeWidth);
					newSession->y = (dfRANGE_MOVE_TOP + 20) + (rand() % safeHeight);
				}
				g_spawnIndex++;
				newSession->hp = 100;

				// 접속 패킷 전송
				auto myPacket = PacketGenerator::MakeCreateMyCharacter(
					newSession->id,
					newSession->direction,
					newSession->x,
					newSession->y,
					newSession->hp
				);
				SendPacket(newSession, (char*)&myPacket, sizeof(myPacket));

				// 다른 유저 생성 패킷 처리 (생략 없이 기존 로직 유지)
				auto otherPacket = PacketGenerator::MakeCreateOtherCharacter(
					newSession->id,
					newSession->direction,
					newSession->x,
					newSession->y,
					newSession->hp
				);
				BroadcastPacket(newSession, (char*)&otherPacket, sizeof(otherPacket));

				for (auto& pair : g_clients) {
					Session* oldUser = pair.second;
					auto oldUserPacket = PacketGenerator::MakeCreateOtherCharacter(
						oldUser->id,
						oldUser->direction,
						oldUser->x,
						oldUser->y,
						oldUser->hp
					);
					SendPacket(newSession, (char*)&oldUserPacket, sizeof(oldUserPacket));
				}
				g_clients[clientSock] = newSession;
			}
		}
		else {
			// [Recv]
			if (g_clients.find(sock) == g_clients.end()) continue;
			Session* session = g_clients[sock];
			char tempRecv[1024];
			int recvLen = recv(sock, tempRecv, 1024, 0);

			if (recvLen <= 0) {
				if (recvLen == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) {}
				else { Disconnect(sock); FD_CLR(sock, &g_readSet); }
			}
			else {
				session->recvBuffer.Enqueue(tempRecv, recvLen);
				while (true) {
					if (session->recvBuffer.GetUseSize() < sizeof(PACKET_HEADER)) break;
					PACKET_HEADER header;
					session->recvBuffer.Peek((char*)&header, sizeof(PACKET_HEADER));
					if (header.byCode != PACKET_CODE) {
						Disconnect(sock); FD_CLR(sock, &g_readSet); break;
					}
					int iTotalPacketSize = sizeof(PACKET_HEADER) + header.bySize;
					if (session->recvBuffer.GetUseSize() < iTotalPacketSize) break;
					char packetData[256];
					session->recvBuffer.Dequeue(packetData, iTotalPacketSize);
					HandlePacket(session, packetData);
				}
			}
		}
	}
}

// -------------------------------------------------------
// 패킷 전송
// -------------------------------------------------------
void SendPacket(Session* session, char* packet, int size) {
	PACKET_HEADER* header = (PACKET_HEADER*)packet;

	// [로그] 전송하는 패킷 정보 출력
	// printf("[Send] ID:%d Type:%d Size:%d\n", session->id, header->byType, size);

	int enqSize = session->sendBuffer.Enqueue(packet, size);
	if (enqSize < size) {
		printf("[Error] SendBuffer Full! Disconnecting User %d\n", session->id);
		Disconnect(session->sock);
	}
}

// -------------------------------------------------------
// [신규] 실제로 데이터를 소켓에 쏘는 함수
// -------------------------------------------------------
void FlushSendBuffer(Session* session) {
	if (session->sendBuffer.GetUseSize() <= 0) return;

	// 1. 버퍼 내용을 보지 않고 가져오기 (Peek)
	// 링버퍼 구현상 GetBufferPtr 같은게 없으므로 Peek로 임시 버퍼에 복사해서 보내거나,
	// 링버퍼 내부 구조를 알면 직접 포인터로 쏘는게 효율적임.
	// 여기서는 Peek -> Send -> Dequeue 방식 사용 (복사 비용 발생하지만 안전함)

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

void BroadcastPacket(Session* excludeSession, char* packet, int size) {
	for (auto& pair : g_clients) {
		if (pair.second == excludeSession) continue;
		SendPacket(pair.second, packet, size);
	}
}

void Disconnect(SOCKET sock) {
	if (g_clients.find(sock) != g_clients.end()) {
		Session* session = g_clients[sock];
		printf("Client Disconnected: ID %d\n", session->id);

		auto packet = PacketGenerator::MakeDeleteCharacter(session->id);

		BroadcastPacket(session, (char*)&packet, sizeof(packet));

		closesocket(sock);
		delete session;
		g_clients.erase(sock);
	}
}

// -------------------------------------------------------
// 패킷 핸들링 (핵심 로직)
// -------------------------------------------------------
void HandlePacket(Session* session, char* packet) {
	PACKET_HEADER* header = (PACKET_HEADER*)packet;

	switch (header->byType) {
	case dfPACKET_CS_MOVE_START:
	{
		P_CS_MOVE_START* pPacket = (P_CS_MOVE_START*)packet;

		// 1. 방향 및 시작 좌표 동기화
		// (보통은 이동 시작 좌표를 신뢰하지만, 오차가 너무 크면 서버 좌표 강제 가능)
		session->direction = pPacket->byDirection;
		session->x = pPacket->sX;
		session->y = pPacket->sY;

		// 2. 서버 시뮬레이션 시작
		session->isMoving = true;

		// 3. 브로드캐스트
		auto sendPacket = PacketGenerator::MakeMoveStart(
			session->id, session->direction, session->x, session->y
		);
		BroadcastPacket(session, (char*)&sendPacket, sizeof(sendPacket));
	}
	break;
	case dfPACKET_CS_MOVE_STOP:
	{
		P_CS_MOVE_STOP* pPacket = (P_CS_MOVE_STOP*)packet;

		// 1. 서버 시뮬레이션 중지
		session->isMoving = false;

		// ★★★ [오차 검증] 서버 좌표 vs 클라 좌표 ★★★
		// 유클리드 거리 계산 (대각선 포함 거리)
		double diff = sqrt(pow(session->x - pPacket->sX, 2) + pow(session->y - pPacket->sY, 2));

		if (diff > dfERROR_RANGE) {
			printf("[CHEAT DETECTED] ID:%d Diff:%.2f (Serv:%d,%d / Client:%d,%d)\n",
				session->id, diff, session->x, session->y, pPacket->sX, pPacket->sY);

			// 오차가 50 이상이면 강제 퇴장 (Disconnect)
			Disconnect(session->sock);
			FD_CLR(session->sock, &g_readSet);
			return;
		}

		// 2. 오차 범위 내라면 클라이언트 좌표를 신뢰하여 보정 (위치 동기화)
		session->direction = pPacket->byDirection;
		session->x = pPacket->sX;
		session->y = pPacket->sY;

		// 3. 브로드캐스트
		auto sendPacket = PacketGenerator::MakeMoveStop(
			session->id, session->direction, session->x, session->y
		);
		BroadcastPacket(session, (char*)&sendPacket, sizeof(sendPacket));
	}
	break;
	case dfPACKET_CS_ATTACK1:
	{
		if (CheckGlobalCooldown(session, 1) == false) return;

		P_CS_ATTACK* pPacket = (P_CS_ATTACK*)packet; // 공격 패킷 구조체 사용

		// 1. 서버 좌표 업데이트 (공격하는 위치 저장)
		session->direction = pPacket->byDirection;
		session->x = pPacket->sX;
		session->y = pPacket->sY;

		// 2. 다른 유저들에게 "얘가 공격1 했다"고 알림
		auto sendPacket = PacketGenerator::MakeAttack1(
			session->id, session->direction, session->x, session->y
		);

		// 나를 제외한 모두에게 전송
		BroadcastPacket(session, (char*)&sendPacket, sizeof(sendPacket));
		ProcAttack(session, 1);
		printf("[Attack1] ID:%d Dir:%d\n", session->id, session->direction);
	}
	break;
	// 공격 2 (X키)
	case dfPACKET_CS_ATTACK2:
	{
		if (CheckGlobalCooldown(session, 2) == false) return;

		P_CS_ATTACK* pPacket = (P_CS_ATTACK*)packet;

		session->direction = pPacket->byDirection;
		session->x = pPacket->sX;
		session->y = pPacket->sY;

		auto sendPacket = PacketGenerator::MakeAttack2(
			session->id, session->direction, session->x, session->y
		);

		BroadcastPacket(session, (char*)&sendPacket, sizeof(sendPacket));
		ProcAttack(session, 2);
		printf("[Attack2] ID:%d Dir:%d\n", session->id, session->direction);
	}
	break;

	// 공격 3 (C키)
	case dfPACKET_CS_ATTACK3:
	{
		if (CheckGlobalCooldown(session, 3) == false) return;

		P_CS_ATTACK* pPacket = (P_CS_ATTACK*)packet;

		session->direction = pPacket->byDirection;
		session->x = pPacket->sX;
		session->y = pPacket->sY;

		auto sendPacket = PacketGenerator::MakeAttack3(
			session->id, session->direction, session->x, session->y
		);

		BroadcastPacket(session, (char*)&sendPacket, sizeof(sendPacket));
		ProcAttack(session, 3);
		printf("[Attack3] ID:%d Dir:%d\n", session->id, session->direction);
	}
	break;

	}
}

// -------------------------------------------------------
// [핵심] 공격 판정 및 데미지 처리 함수
// -------------------------------------------------------
void ProcAttack(Session* attacker, int attackType) {
	// 1. 공격 타입에 따른 범위와 데미지 설정
	int rangeX = 0;
	int rangeY = 0;
	int damage = 0;

	switch (attackType) {
	case 1:
		rangeX = dfATTACK1_RANGE_X; // 80
		rangeY = dfATTACK1_RANGE_Y; // 10
		damage = dfDAMAGE1;
		break;
	case 2:
		rangeX = dfATTACK2_RANGE_X; // 90
		rangeY = dfATTACK2_RANGE_Y; // 10
		damage = dfDAMAGE2;
		break;
	case 3:
		rangeX = dfATTACK3_RANGE_X; // 100
		rangeY = dfATTACK3_RANGE_Y; // 20
		damage = dfDAMAGE3;
		break;
	default:
		return;
	}

	// 2. 공격자 방향에 따른 X축 히트박스 계산
	// 0,1,2,7 : 왼쪽 / 3,4,5,6 : 오른쪽
	int minX, maxX;
	int minY, maxY;

	if (attacker->direction == dfPACKET_MOVE_DIR_LL)
	{
		// 왼쪽을 보고 있음: (내 위치 - 사거리) ~ (내 위치)
		minX = attacker->x - rangeX;
		maxX = attacker->x;
	}
	else
	{
		// 오른쪽을 보고 있음: (내 위치) ~ (내 위치 + 사거리)
		minX = attacker->x;
		maxX = attacker->x + rangeX;
	}

	// Y축 히트박스 (내 위치 기준 위아래)
	minY = attacker->y - rangeY;
	maxY = attacker->y + rangeY;

	std::vector<SOCKET> deadSockets;

	// 3. 모든 유저를 순회하며 충돌 검사
	for (auto& pair : g_clients) {
		Session* victim = pair.second;

		// 나 자신은 때리지 않음
		if (victim == attacker) continue;

		// 이미 죽은 사람은 때리지 않음
		if (victim->hp <= 0) continue;

		// ★ 충돌 체크 (AABB) ★
		// 상대방의 좌표가 내 공격 범위 사각형 안에 들어왔는가?
		if (victim->x >= minX && victim->x <= maxX &&
			victim->y >= minY && victim->y <= maxY)
		{
			// 4. 데미지 적용
			// HP 감소 (0 미만으로 내려가지 않게 처리)
			if (victim->hp < damage) victim->hp = 0;
			else victim->hp -= damage;

			printf("[HIT!] Attacker:%d -> Victim:%d (HP:%d)\n", attacker->id, victim->id, victim->hp);

			// 5. 데미지 패킷 브로드캐스팅 (SC_DAMAGE)
			auto damagePacket = PacketGenerator::MakeDamage(
				attacker->id,
				victim->id,
				victim->hp
			);

			BroadcastPacket(nullptr, (char*)&damagePacket, sizeof(damagePacket)); // 모두에게 전송

			if (victim->hp <= 0) {
				// 여기서 바로 Disconnect 하면 맵 루프가 터짐!
				// 명단에만 적어둠
				deadSockets.push_back(victim->sock);
			}
		}
	}
	for (SOCKET sock : deadSockets) {
		// Disconnect 함수 내부에서:
		// 1. P_SC_DELETE_CHARACTER 패킷을 다른 유저들에게 브로드캐스트
		// 2. 소켓 close
		// 3. g_clients 맵에서 제거
		// 가 모두 수행됩니다.
		printf("[DEATH] User %d HP is 0. Disconnecting...\n", g_clients[sock]->id);
		Disconnect(sock);

		// 주의: Disconnect 후에는 sock이 readSet에서도 빠져야 안전함
		// g_readSet은 전역변수이므로 Disconnect 안에서 처리 안 했다면 여기서 처리
		FD_CLR(sock, &g_readSet);
	}
}

