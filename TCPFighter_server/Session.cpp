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

Session::Session() 	: sock(INVALID_SOCKET), 
	  id(-1), 
	  x(0), 
	  y(0), 
	  direction(0), 
	  hp(100), 
	  isMoving(false), 
	  nextActionEnableTime(0) 
{
	recvBuffer.Resize(10240);
	sendBuffer.Resize(10240);
}