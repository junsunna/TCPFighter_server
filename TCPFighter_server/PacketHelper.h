#pragma once

#define PACKET_CODE 0x89

class PacketGenerator {
private:
	// [내부용] 헤더 초기화 헬퍼 함수
	static void InitHeader(PACKET_HEADER& header, unsigned char type, unsigned char size);

public:
	// -------------------------------------------------------
	// 1. 캐릭터 생성 관련 (접속 시)
	// -------------------------------------------------------

	// [내 캐릭터 생성] 본인에게 전송
	static P_SC_CREATE_MY_CHARACTER MakeCreateMyCharacter(int id, unsigned char dir, short x, short y, unsigned char hp);
	// [다른 캐릭터 생성] 주변 유저들에게 전송 / 내가 접속했을 때 주변 유저 정보 수신
	static P_SC_CREATE_OTHER_CHARACTER MakeCreateOtherCharacter(int id, unsigned char dir, short x, short y, unsigned char hp);
	// [캐릭터 삭제] 퇴장 시 주변 유저들에게 전송
	static P_SC_DELETE_CHARACTER MakeDeleteCharacter(int id);
	// -------------------------------------------------------
	// 2. 이동 관련
	// -------------------------------------------------------

	// [이동 시작]
	static P_SC_MOVE_START MakeMoveStart(int id, unsigned char dir, short x, short y);
	// [이동 멈춤]
	static P_SC_MOVE_STOP MakeMoveStop(int id, unsigned char dir, short x, short y);
	// -------------------------------------------------------
	// 3. 전투/공격 관련
	// -------------------------------------------------------

	// [공격 1]
	static P_SC_ATTACK MakeAttack1(int id, unsigned char dir, short x, short y);
	// [공격 2]
	static P_SC_ATTACK MakeAttack2(int id, unsigned char dir, short x, short y);
	// [공격 3]
	static P_SC_ATTACK MakeAttack3(int id, unsigned char dir, short x, short y);
	// [피격/데미지]
	static P_SC_DAMAGE MakeDamage(int attackerID, int victimID, unsigned char remainHP);
};