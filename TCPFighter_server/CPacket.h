#pragma once

class CPacket {
public:
	CPacket(size_t initialSize = 1024);
	virtual ~CPacket();

	void Clear();

	void PutData(const char* pData, size_t size);

	template<typename T>
	CPacket& operator<<(const T& data)
	{
		PutData(reinterpret_cast<const char*>(&data), sizeof(T));
		return *this;
	}

	CPacket& operator<<(const std::string& str);
	CPacket& operator<<(const char& str);

	void GetData(char* pDest, size_t size);

	template<typename T>
	CPacket& operator>>(T& data)
	{
		GetData(reinterpret_cast<char*>(&data), sizeof(T));
		return *this;
	}

	CPacket& operator>>(std::string& str);

	char* GetBufferPtr();
	size_t GetSize() const;
protected:
	std::vector<char> m_buffer;
	size_t m_readPos;
};