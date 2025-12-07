#pragma once

#define PACKET_CODE 0x89

class PacketGenerator {
private:
	// [내부용] 헤더를 쓰고, 패킷 작성을 마무리는 헬퍼 함수
	static void WriteHeader(CPacket* packet, unsigned char type);
	static void FinishPacket(CPacket* packet);

public:
	// -------------------------------------------------------
	// 1. 캐릭터 생성 관련
	// -------------------------------------------------------
	static void MakeCreateMyCharacter(CPacket* packet, int id, unsigned char dir, short x, short y, unsigned char hp);
	static void MakeCreateOtherCharacter(CPacket* packet, int id, unsigned char dir, short x, short y, unsigned char hp);
	static void MakeDeleteCharacter(CPacket* packet, int id);

	// -------------------------------------------------------
	// 2. 이동 관련
	// -------------------------------------------------------
	static void MakeMoveStart(CPacket* packet, int id, unsigned char dir, short x, short y);
	static void MakeMoveStop(CPacket* packet, int id, unsigned char dir, short x, short y);

	// -------------------------------------------------------
	// 3. 전투/공격 관련
	// -------------------------------------------------------
	static void MakeAttack1(CPacket* packet, int id, unsigned char dir, short x, short y);
	static void MakeAttack2(CPacket* packet, int id, unsigned char dir, short x, short y);
	static void MakeAttack3(CPacket* packet, int id, unsigned char dir, short x, short y);
	static void MakeDamage(CPacket* packet, int attackerID, int victimID, unsigned char remainHP);
};