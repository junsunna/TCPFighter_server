#include "PacketDefine.h"  // 패킷 타입(dfPACKET_...) 정의
#include "PacketStructs.h"
#include "PacketHelper.h"


// [내부용] 헤더 초기화 헬퍼 함수
void PacketGenerator::InitHeader(PACKET_HEADER& header, unsigned char type, unsigned char size) {
	header.byCode = PACKET_CODE;
	header.byType = type;
	header.bySize = size; // BYTE로 캐스팅
}

// -------------------------------------------------------
// 1. 캐릭터 생성 관련 (접속 시)
// -------------------------------------------------------

// [내 캐릭터 생성] 본인에게 전송
P_SC_CREATE_MY_CHARACTER PacketGenerator::MakeCreateMyCharacter(int id, unsigned char dir, short x, short y, unsigned char hp) {
	P_SC_CREATE_MY_CHARACTER packet;
	InitHeader(packet.header, dfPACKET_SC_CREATE_MY_CHARACTER, sizeof(P_SC_CREATE_MY_CHARACTER) - sizeof(PACKET_HEADER));

	packet.iID = id;
	packet.byDirection = dir;
	packet.sX = x;
	packet.sY = y;
	packet.byHP = hp;
	return packet;
}

// [다른 캐릭터 생성] 주변 유저들에게 전송 / 내가 접속했을 때 주변 유저 정보 수신
P_SC_CREATE_OTHER_CHARACTER PacketGenerator::MakeCreateOtherCharacter(int id, unsigned char dir, short x, short y, unsigned char hp) {
	P_SC_CREATE_OTHER_CHARACTER packet;
	InitHeader(packet.header, dfPACKET_SC_CREATE_OTHER_CHARACTER, sizeof(P_SC_CREATE_OTHER_CHARACTER) - sizeof(PACKET_HEADER));

	packet.iID = id;
	packet.byDirection = dir;
	packet.sX = x;
	packet.sY = y;
	packet.byHP = hp;
	return packet;
}

// [캐릭터 삭제] 퇴장 시 주변 유저들에게 전송
P_SC_DELETE_CHARACTER PacketGenerator::MakeDeleteCharacter(int id) {
	P_SC_DELETE_CHARACTER packet;
	InitHeader(packet.header, dfPACKET_SC_DELETE_CHARACTER, sizeof(P_SC_DELETE_CHARACTER) - sizeof(PACKET_HEADER));

	packet.iID = id;
	return packet;
}

// -------------------------------------------------------
// 2. 이동 관련
// -------------------------------------------------------

// [이동 시작]
P_SC_MOVE_START PacketGenerator::MakeMoveStart(int id, unsigned char dir, short x, short y) {
	P_SC_MOVE_START packet;
	InitHeader(packet.header, dfPACKET_SC_MOVE_START, sizeof(P_SC_MOVE_START) - sizeof(PACKET_HEADER));

	packet.iID = id;
	packet.byDirection = dir;
	packet.sX = x;
	packet.sY = y;
	return packet;
}

// [이동 멈춤]
P_SC_MOVE_STOP PacketGenerator::MakeMoveStop(int id, unsigned char dir, short x, short y) {
	P_SC_MOVE_STOP packet;
	InitHeader(packet.header, dfPACKET_SC_MOVE_STOP, sizeof(P_SC_MOVE_STOP) - sizeof(PACKET_HEADER));

	packet.iID = id;
	packet.byDirection = dir;
	packet.sX = x;
	packet.sY = y;
	return packet;
}

// -------------------------------------------------------
// 3. 전투/공격 관련
// -------------------------------------------------------

// [공격 1]
P_SC_ATTACK PacketGenerator::MakeAttack1(int id, unsigned char dir, short x, short y) {
	P_SC_ATTACK packet;
	InitHeader(packet.header, dfPACKET_SC_ATTACK1, sizeof(P_SC_ATTACK) - sizeof(PACKET_HEADER));

	packet.iID = id;
	packet.byDirection = dir;
	packet.sX = x;
	packet.sY = y;
	return packet;
}

// [공격 2]
P_SC_ATTACK PacketGenerator::MakeAttack2(int id, unsigned char dir, short x, short y) {
	P_SC_ATTACK packet;
	InitHeader(packet.header, dfPACKET_SC_ATTACK2, sizeof(P_SC_ATTACK) - sizeof(PACKET_HEADER));

	packet.iID = id;
	packet.byDirection = dir;
	packet.sX = x;
	packet.sY = y;
	return packet;
}

// [공격 3]
P_SC_ATTACK PacketGenerator::MakeAttack3(int id, unsigned char dir, short x, short y) {
	P_SC_ATTACK packet;
	InitHeader(packet.header, dfPACKET_SC_ATTACK3, sizeof(P_SC_ATTACK) - sizeof(PACKET_HEADER));

	packet.iID = id;
	packet.byDirection = dir;
	packet.sX = x;
	packet.sY = y;
	return packet;
}

// [피격/데미지]
P_SC_DAMAGE PacketGenerator::MakeDamage(int attackerID, int victimID, unsigned char remainHP) {
	P_SC_DAMAGE packet;
	InitHeader(packet.header, dfPACKET_SC_DAMAGE, sizeof(P_SC_DAMAGE) - sizeof(PACKET_HEADER));

	packet.iAttackID = attackerID;
	packet.iDamageID = victimID;
	packet.byDamageHP = remainHP;
	return packet;
}
