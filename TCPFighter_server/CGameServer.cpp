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
#include "Session.h"
#include "CPacket.h"

#include "PacketStructs.h"
#include "CNetworkManager.h"
#include "PacketHelper.h"
#include "CGameServer.h"


// 게임 상수 정의 (필요하면 헤더나 Define.h로 분리)
#define dfRANGE_MOVE_TOP    50
#define dfRANGE_MOVE_LEFT   10
#define dfRANGE_MOVE_RIGHT  630
#define dfRANGE_MOVE_BOTTOM 470
#define dfERROR_RANGE       50

// 이동 속도
#define dfSPEED_X           3
#define dfSPEED_Y           2

// 공격 쿨타임
#define dfDELAY_ATTACK1  200   
#define dfDELAY_ATTACK2  200  
#define dfDELAY_ATTACK3  400 

// 데미지
#define dfDAMAGE1			5
#define dfDAMAGE2			7
#define dfDAMAGE3			10

// 공격 범위
#define dfATTACK1_RANGE_X 80
#define dfATTACK1_RANGE_Y 10

#define dfATTACK2_RANGE_X 90
#define dfATTACK2_RANGE_Y 10

#define dfATTACK3_RANGE_X 100
#define dfATTACK3_RANGE_Y 20

CGameServer::CGameServer() : m_network(nullptr) {}
CGameServer::~CGameServer() {}

// -------------------------------------------------------------------------
// 1. [반응형] 패킷 처리 (Switch문)
// -------------------------------------------------------------------------
void CGameServer::HandlePacket(Session* session, CPacket& packet) {
	// CPacket에서 헤더 정보를 읽어옵니다.
	// (CPacket 구현에 따라 >> 로 읽거나, 포인터로 읽습니다)
	PACKET_HEADER header;
	char* ptr = packet.GetBufferPtr();
	memcpy(&header, ptr, sizeof(PACKET_HEADER));

	// 데이터를 읽기 위해 커서를 헤더 뒤로 넘겨야 합니다.
	// (단순하게 구현하기 위해 임시로 헤더만큼 값을 빼냅니다)
	PACKET_HEADER dummy;
	packet >> dummy.byCode >> dummy.byType >> dummy.bySize;

	// ★★★ 여기서 로직 분기 ★★★
	switch (header.byType) {
	case dfPACKET_CS_MOVE_START:
	{
		unsigned char dir;
		short x, y;
		packet >> dir >> x >> y; // 직관적인 데이터 추출

		session->direction = dir;
		session->x = x;
		session->y = y;
		session->isMoving = true;

		// 다른 유저들에게 브로드캐스팅
		CPacket sendPacket;
		PacketGenerator::MakeMoveStart(&sendPacket, session->id, dir, x, y);
		m_network->BroadcastPacket(session, &sendPacket);
	}
	break;

	case dfPACKET_CS_MOVE_STOP:
	{
		unsigned char dir;
		short x, y;
		packet >> dir >> x >> y;

		session->isMoving = false; // 이동 멈춤

		// [오차 검증]
		double diff = sqrt(pow(session->x - x, 2) + pow(session->y - y, 2));
		if (diff > dfERROR_RANGE) {
			printf("[CHEAT] User %d Position Error (Diff: %.2f)\n", session->id, diff);
			m_network->Disconnect(session->sock); // 강퇴 요청
			return;
		}

		// 좌표 동기화
		session->direction = dir;
		session->x = x;
		session->y = y;

		CPacket sendPacket;
		PacketGenerator::MakeMoveStop(&sendPacket, session->id, dir, x, y);
		m_network->BroadcastPacket(session, &sendPacket);
	}
	break;

	case dfPACKET_CS_ATTACK1:
	{
		// 쿨타임 체크
		if (CheckGlobalCooldown(session, 1) == false) return;

		unsigned char dir;
		short x, y;
		packet >> dir >> x >> y;

		// 좌표 업데이트 및 공격 처리
		session->direction = dir;
		session->x = x;
		session->y = y;

		session->isMoving = false;

		// 공격 모션 브로드캐스트
		CPacket sendPacket;
		PacketGenerator::MakeAttack1(&sendPacket, session->id, dir, x, y);
		m_network->BroadcastPacket(session, &sendPacket);

		// 실제 데미지 판정 로직 호출
		ProcAttack(session, 1);
	}
	break;
	// 공격 2 (X키)
	case dfPACKET_CS_ATTACK2:
	{
		if (CheckGlobalCooldown(session, 2) == false) return;

		unsigned char dir;
		short x, y;
		packet >> dir >> x >> y;

		// 좌표 업데이트 및 공격 처리
		session->direction = dir;
		session->x = x;
		session->y = y;

		session->isMoving = false;

		// 공격 모션 브로드캐스트
		CPacket sendPacket;
		PacketGenerator::MakeAttack2(&sendPacket, session->id, dir, x, y);
		m_network->BroadcastPacket(session, &sendPacket);

		// 실제 데미지 판정 로직 호출
		ProcAttack(session, 2);
	}
	break;

	// 공격 3 (C키)
	case dfPACKET_CS_ATTACK3:
	{
		if (CheckGlobalCooldown(session, 3) == false) return;

		unsigned char dir;
		short x, y;
		packet >> dir >> x >> y;

		// 좌표 업데이트 및 공격 처리
		session->direction = dir;
		session->x = x;
		session->y = y;

		session->isMoving = false;

		// 공격 모션 브로드캐스트
		CPacket sendPacket;
		PacketGenerator::MakeAttack3(&sendPacket, session->id, dir, x, y);

		m_network->BroadcastPacket(session, &sendPacket);
		ProcAttack(session, 3);
	}
	break;
	}
}

// -------------------------------------------------------------------------
// 2. [주기적] 프레임 업데이트 (이동 시뮬레이션)
// -------------------------------------------------------------------------
void CGameServer::Update(double dt) {
	if (m_network == nullptr) return;

	// 네트워크 매니저에게서 현재 접속 중인 유저 목록을 가져옴
	auto& clients = m_network->GetClients();

	for (auto& pair : clients) {
		Session* session = pair.second;

		// 이동 중인 유저만 좌표 계산
		if (session->isMoving) {
			MoveCharacter(session, dt);
			std::cout << "User " << session->id << " moved to (" << session->x << ", " << session->y << ")\n";
		}
	}
}

// -------------------------------------------------------------------------
// 3. 내부 로직 함수들 (이동, 공격 판정)
// -------------------------------------------------------------------------
void CGameServer::MoveCharacter(Session* session, double dt) {
	double moveFactor = dt * 50.0;
	double nextX = (double)session->x;
	double nextY = (double)session->y;

	double tempX = nextX;

	// 방향에 따른 X축 이동량 계산
	if (session->direction == dfPACKET_MOVE_DIR_LL ||
		session->direction == dfPACKET_MOVE_DIR_LU ||
		session->direction == dfPACKET_MOVE_DIR_LD) {
		tempX -= (dfSPEED_X * moveFactor);
	}
	else if (session->direction == dfPACKET_MOVE_DIR_RR ||
		session->direction == dfPACKET_MOVE_DIR_RU ||
		session->direction == dfPACKET_MOVE_DIR_RD) {
		tempX += (dfSPEED_X * moveFactor);
	}

	// ★ 핵심: 벽을 넘었으면 벽 좌표로 고정 (Clamping)
	if (tempX < dfRANGE_MOVE_LEFT) {
		tempX = dfRANGE_MOVE_LEFT; // 왼쪽 벽에 딱 붙임
	}
	else if (tempX > dfRANGE_MOVE_RIGHT) {
		tempX = dfRANGE_MOVE_RIGHT; // 오른쪽 벽에 딱 붙임
	}

	nextX = tempX; // 보정된 X좌표 확정

	double tempY = nextY;

	// 방향에 따른 Y축 이동량 계산
	if (session->direction == dfPACKET_MOVE_DIR_UU ||
		session->direction == dfPACKET_MOVE_DIR_LU ||
		session->direction == dfPACKET_MOVE_DIR_RU) {
		tempY -= (dfSPEED_Y * moveFactor);
	}
	else if (session->direction == dfPACKET_MOVE_DIR_DD ||
		session->direction == dfPACKET_MOVE_DIR_LD ||
		session->direction == dfPACKET_MOVE_DIR_RD) {
		tempY += (dfSPEED_Y * moveFactor);
	}

	// ★ 핵심: 벽을 넘었으면 벽 좌표로 고정
	if (tempY < dfRANGE_MOVE_TOP) {
		tempY = dfRANGE_MOVE_TOP; // 위쪽 벽에 딱 붙임
	}
	else if (tempY > dfRANGE_MOVE_BOTTOM) {
		tempY = dfRANGE_MOVE_BOTTOM; // 아래쪽 벽에 딱 붙임
	}

	nextY = tempY; // 보정된 Y좌표 확정

	session->x = (short)nextX;
	session->y = (short)nextY;
}

void CGameServer::ProcAttack(Session* attacker, int attackType) {
	auto& clients = m_network->GetClients();
	// 1. 공격 타입에 따른 범위와 데미지 설정
	int rangeX = 0;
	int rangeY = 0;
	int damage = 0;

	switch (attackType) {
	case 1:
		rangeX = dfATTACK1_RANGE_X; 
		rangeY = dfATTACK1_RANGE_Y; 
		damage = dfDAMAGE1;
		break;
	case 2:
		rangeX = dfATTACK2_RANGE_X;
		rangeY = dfATTACK2_RANGE_Y; 
		damage = dfDAMAGE2;
		break;
	case 3:
		rangeX = dfATTACK3_RANGE_X;
		rangeY = dfATTACK3_RANGE_Y;
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
	for (auto& pair : clients) {
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

			// 5. 데미지 패킷 브로드캐스팅 (SC_DAMAGE)
			CPacket damagePacket;
			PacketGenerator::MakeDamage(&damagePacket, attacker->id, victim->id, victim->hp);
			m_network->BroadcastPacket(nullptr, &damagePacket); 

			if (victim->hp <= 0) {
				deadSockets.push_back(victim->sock);
			}
		}
	}
	for (SOCKET sock : deadSockets) {
		m_network->Disconnect(sock);
	}
}

bool CGameServer::CheckGlobalCooldown(Session* session, int attackType) {
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