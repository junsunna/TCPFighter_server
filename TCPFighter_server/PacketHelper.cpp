#include <WinSock2.h>
#include <algorithm>
#include <cstring> 
#include <cstdint> 
#include <vector>
#include <cmath>
#include <iostream>
#include <unordered_map>

#include "PacketStructs.h"
#include "CPacket.h"
#include "PacketDefine.h"  // 패킷 타입(dfPACKET_...) 정의

#include "PacketHelper.h"

void PacketGenerator::WriteHeader(CPacket* packet, unsigned char type) {
	// 1. 재사용을 위해 버퍼 초기화
	packet->Clear();

	// 2. 헤더 구조체를 만들어서 맨 앞에 넣습니다.
	// (이때 사이즈는 아직 모르므로 0으로 넣습니다.)
	PACKET_HEADER header;
	header.byCode = PACKET_CODE;
	header.byType = type;
	header.bySize = 0; // 나중에 FinishPacket에서 채움

	// 헤더를 버퍼에 씁니다.
	packet->PutData((char*)&header, sizeof(PACKET_HEADER));
}

void PacketGenerator::FinishPacket(CPacket* packet) {
	// 패킷 작성이 끝났으므로, 헤더의 bySize 부분을 계산해서 채워줍니다.

	// 1. 버퍼의 시작 주소(헤더가 있는 곳)를 가져옵니다.
	PACKET_HEADER* pHeader = (PACKET_HEADER*)packet->GetBufferPtr();

	// 2. 전체 크기 - 헤더 크기 = 바디(Payload) 크기
	// (CPacket::GetSize()는 현재 버퍼에 들어있는 전체 바이트 수)
	pHeader->bySize = (unsigned char)(packet->GetSize() - sizeof(PACKET_HEADER));
}


// ------------------------------------------------------------------
// 1. 캐릭터 생성 관련
// ------------------------------------------------------------------

void PacketGenerator::MakeCreateMyCharacter(CPacket* packet, int id, unsigned char dir, short x, short y, unsigned char hp) {
	WriteHeader(packet, dfPACKET_SC_CREATE_MY_CHARACTER); // 헤더 쓰기

	// 데이터 직렬화 (순서 중요!)
	*packet << id;
	*packet << dir;
	*packet << x;
	*packet << y;
	*packet << hp;

	FinishPacket(packet); // 사이즈 계산 후 헤더 갱신
}

void PacketGenerator::MakeCreateOtherCharacter(CPacket* packet, int id, unsigned char dir, short x, short y, unsigned char hp) {
	WriteHeader(packet, dfPACKET_SC_CREATE_OTHER_CHARACTER);

	*packet << id << dir << x << y << hp; // 이렇게 체이닝도 가능합니다 (CPacket 구현에 따라)

	FinishPacket(packet);
}

void PacketGenerator::MakeDeleteCharacter(CPacket* packet, int id) {
	WriteHeader(packet, dfPACKET_SC_DELETE_CHARACTER);

	*packet << id;

	FinishPacket(packet);
}


// ------------------------------------------------------------------
// 2. 이동 관련
// ------------------------------------------------------------------

void PacketGenerator::MakeMoveStart(CPacket* packet, int id, unsigned char dir, short x, short y) {
	WriteHeader(packet, dfPACKET_SC_MOVE_START);

	*packet << id;
	*packet << dir;
	*packet << x;
	*packet << y;

	FinishPacket(packet);
}

void PacketGenerator::MakeMoveStop(CPacket* packet, int id, unsigned char dir, short x, short y) {
	WriteHeader(packet, dfPACKET_SC_MOVE_STOP);

	*packet << id;
	*packet << dir;
	*packet << x;
	*packet << y;

	FinishPacket(packet);
}


// ------------------------------------------------------------------
// 3. 전투/공격 관련
// ------------------------------------------------------------------

void PacketGenerator::MakeAttack1(CPacket* packet, int id, unsigned char dir, short x, short y) {
	WriteHeader(packet, dfPACKET_SC_ATTACK1);
	*packet << id << dir << x << y;
	FinishPacket(packet);
}

void PacketGenerator::MakeAttack2(CPacket* packet, int id, unsigned char dir, short x, short y) {
	WriteHeader(packet, dfPACKET_SC_ATTACK2);
	*packet << id << dir << x << y;
	FinishPacket(packet);
}

void PacketGenerator::MakeAttack3(CPacket* packet, int id, unsigned char dir, short x, short y) {
	WriteHeader(packet, dfPACKET_SC_ATTACK3);
	*packet << id << dir << x << y;
	FinishPacket(packet);
}

void PacketGenerator::MakeDamage(CPacket* packet, int attackerID, int victimID, unsigned char remainHP) {
	WriteHeader(packet, dfPACKET_SC_DAMAGE);

	*packet << attackerID;
	*packet << victimID;
	*packet << remainHP;

	FinishPacket(packet);
}
