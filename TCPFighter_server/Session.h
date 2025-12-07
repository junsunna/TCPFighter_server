#pragma once


// 세션 구조체
struct Session {
	Session();
	SOCKET sock;
	int id;

	// 게임 관련 데이터
	short x, y;
	unsigned char direction;
	unsigned char hp;
	bool isMoving;

	// 네트워크 버퍼
	RingBuffer recvBuffer;
	RingBuffer sendBuffer;

	// 쿨타임 관리
	ULONGLONG nextActionEnableTime;
};