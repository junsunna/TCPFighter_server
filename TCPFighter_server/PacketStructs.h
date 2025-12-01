#pragma once
#include "PacketDefine.h"

#pragma pack(push, 1) // 1바이트 정렬 시작

// 공통 헤더
struct PACKET_HEADER {
	unsigned char byCode; // 0x89
	unsigned char bySize; // 전체 패킷 사이즈
	unsigned char byType; // 메시지 타입 (dfPACKET_...)
};

// -------------------------------------------------
// [Client -> Server] 패킷
// -------------------------------------------------

// dfPACKET_CS_MOVE_START (10)
struct P_CS_MOVE_START {
	PACKET_HEADER header;
	unsigned char byDirection;
	short sX;
	short sY;
};

// dfPACKET_CS_MOVE_STOP (12)
struct P_CS_MOVE_STOP {
	PACKET_HEADER header;
	unsigned char byDirection;
	short sX;
	short sY;
};
// dfPACKET_CS_ATTACK1 (20), ATTACK2 (22), ATTACK3 (24)
// 공격 1, 2, 3 모두 데이터 구조가 같으므로 공용으로 사용
struct P_CS_ATTACK {
	PACKET_HEADER header;
	unsigned char byDirection;
	short sX;
	short sY;
};

// dfPACKET_CS_SYNC (250) - (사용 안 함으로 되어있으나 정의)
struct P_CS_SYNC {
	PACKET_HEADER header;
	short sX;
	short sY;
};
// -------------------------------------------------
// [Server -> Client] 패킷
// -------------------------------------------------

// dfPACKET_SC_CREATE_MY_CHARACTER (0)
struct P_SC_CREATE_MY_CHARACTER {
	PACKET_HEADER header;
	int iID;
	unsigned char byDirection;
	short sX;
	short sY;
	unsigned char byHP;
};

// dfPACKET_SC_CREATE_OTHER_CHARACTER (1)
struct P_SC_CREATE_OTHER_CHARACTER {
	PACKET_HEADER header;
	int iID;
	unsigned char byDirection;
	short sX;
	short sY;
	unsigned char byHP;
};

// dfPACKET_SC_DELETE_CHARACTER (2)
struct P_SC_DELETE_CHARACTER {
	PACKET_HEADER header;
	int iID;
};

// dfPACKET_SC_MOVE_START (11)
struct P_SC_MOVE_START {
	PACKET_HEADER header;
	int iID;
	unsigned char byDirection;
	short sX;
	short sY;
};

// dfPACKET_SC_MOVE_STOP (13)
struct P_SC_MOVE_STOP {
	PACKET_HEADER header;
	int iID;
	unsigned char byDirection;
	short sX;
	short sY;
};

// dfPACKET_SC_ATTACK1 (21), ATTACK2 (23), ATTACK3 (25)
// 공격 1, 2, 3 모두 데이터 구조가 같으므로 공용으로 사용
struct P_SC_ATTACK {
	PACKET_HEADER header;
	int iID;
	unsigned char byDirection;
	short sX;
	short sY;
};

// dfPACKET_SC_DAMAGE (30)
struct P_SC_DAMAGE {
	PACKET_HEADER header;
	int iAttackID;  // 공격자 ID
	int iDamageID;  // 피해자 ID
	unsigned char byDamageHP; // 남은 HP
};

// dfPACKET_SC_SYNC (251)
struct P_SC_SYNC {
	PACKET_HEADER header;
	int iID;
	short sX;
	short sY;
};

#pragma pack(pop) // 정렬 복구