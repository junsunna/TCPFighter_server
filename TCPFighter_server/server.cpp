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
#include "CNetworkManager.h"
#include "CGameServer.h"

#define SERVER_PORT 5000
#define GAME_FPS    50

int main() {
	CNetworkManager network;
	CGameServer gameServer;

	gameServer.SetNetworkManager(&network);

	if (!network.Init(SERVER_PORT, &gameServer)) {
		std::cout << "[Main] Server Init Failed.\n";
		return 0;
	}

	LARGE_INTEGER frequency;
	LARGE_INTEGER prevTime, currTime;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&prevTime);

	double timeScale = 1.0 / frequency.QuadPart;
	
	const double TARGET_FRAME_TIME = 1.0 / GAME_FPS;

	double timeAccumulator = 0.0;
	
	while (true) {
		QueryPerformanceCounter(&currTime);
		double dt = (currTime.QuadPart - prevTime.QuadPart) * timeScale;
		prevTime = currTime;

		if (dt > 0.1) {
			dt = 0.1;
		}

		network.NetworkProc();

		timeAccumulator += dt;

		// 누적 시간(accumulator)에 0.1초가 들어있음
		if (timeAccumulator >= TARGET_FRAME_TIME) {
			gameServer.Update(timeAccumulator); // 딱 한 번만 0.02초치 이동함
			timeAccumulator = 0;
		}

	}

	network.CleanUp();

	return 0;
}