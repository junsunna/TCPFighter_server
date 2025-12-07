#include <WinSock2.h>
#include <algorithm>
#include <cstring> 
#include <cstdint> 
#include <vector>
#include <cmath>
#include <iostream>
#include <unordered_map>

#pragma comment(lib, "ws2_32.lib")

#include "CPacket.h"

CPacket::CPacket(size_t initialSize) : m_readPos(0)
{
	m_buffer.reserve(initialSize);
}

CPacket::~CPacket()
{
	Clear();
}

void CPacket::Clear()
{
	m_buffer.clear();
	m_readPos = 0;
}

void CPacket::PutData(const char* pData, size_t size)
{
	m_buffer.insert(m_buffer.end(), pData, pData + size);
}

CPacket& CPacket::operator<<(const std::string& str)
{
	uint8_t length = static_cast<uint16_t>(str.size());
	*this << length;
	PutData(str.c_str(), length);
	return *this;
}

CPacket& CPacket::operator<<(const char& str)
{
	uint16_t length = static_cast<uint16_t>(1);
	*this << length;
	PutData(&str, length);
	return *this;
}

void CPacket::GetData(char* pDest, size_t size)
{
	if (m_readPos + size > m_buffer.size())
	{
		memset(pDest, 0, size);
		return;
	}
	memcpy(pDest, &m_buffer[m_readPos], size);

	m_readPos += size;
}

CPacket& CPacket::operator>>(std::string& str)
{
	uint8_t len = 0;

	*this >> len;

	str.resize(len);

	if (len > 0)
	{
		GetData(&str[0], len);
	}

	return *this;
}

char* CPacket::GetBufferPtr()
{
	if (m_buffer.empty()) return nullptr;
	return &m_buffer[0];
}

size_t CPacket::GetSize() const
{
	return m_buffer.size();
}