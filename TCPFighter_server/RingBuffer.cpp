#include "RingBuffer.h"
#include <algorithm> // std::min
#include <cstring>   // memcpy

// -------- 생성자 및 소멸자 --------

RingBuffer::RingBuffer(void)
	: m_pBuffer(nullptr)
	, m_iBufferSize(0)
	, m_iFront(0)
	, m_iRear(0)
{
	// 기본 크기 설정 (예: 10240)
	// 필요하다면 여기서 초기화를 하거나, 빈 상태로 두고 Resize를 호출하게 할 수도 있음
	Resize(10240);
}

RingBuffer::RingBuffer(int iBufferSize)
	: m_pBuffer(nullptr)
	, m_iBufferSize(0)
	, m_iFront(0)
	, m_iRear(0)
{
	Resize(iBufferSize);
}

RingBuffer::~RingBuffer(void)
{
	if (m_pBuffer != nullptr)
	{
		delete[] m_pBuffer;
		m_pBuffer = nullptr;
	}
}

// -------- 초기화 및 리사이징 --------

void RingBuffer::Resize(int iSize)
{
	// 방어 코드: 기존 크기보다 작거나 같으면 정책에 따라 리턴하거나 수행
	// 여기서는 단순히 재할당 로직만 구현
	if (iSize <= 0) return;

	// 1. 새 버퍼 할당
	char* pNewBuffer = new char[iSize];

	// 2. 기존 데이터가 있다면 새 버퍼로 '일렬로 펴서(Linearize)' 복사
	if (m_pBuffer != nullptr)
	{
		int iUsedSize = GetUseSize();

		// 새 버퍼가 기존 데이터보다 작으면 잘릴 수 있음 (방어)
		int iCopySize = (iSize < iUsedSize) ? iSize : iUsedSize;

		if (iCopySize > 0)
		{
			// Peek 함수 로직을 응용하여 복사 (Front부터 순서대로)
			// 여기서는 내부 변수에 직접 접근하므로 수동으로 복사
			int iDirectRead = m_iBufferSize - m_iFront;

			if (iCopySize <= iDirectRead)
			{
				// 끊어지지 않은 경우 한 번에 복사
				memcpy(pNewBuffer, &m_pBuffer[m_iFront], iCopySize);
			}
			else
			{
				// 끊어진 경우 두 번에 나누어 복사
				memcpy(pNewBuffer, &m_pBuffer[m_iFront], iDirectRead);
				memcpy(pNewBuffer + iDirectRead, &m_pBuffer[0], iCopySize - iDirectRead);
			}
		}

		// 3. 기존 버퍼 해제
		delete[] m_pBuffer;

		// 4. 멤버 변수 갱신 (Front는 0으로 초기화, Rear는 데이터 끝으로 이동)
		m_iFront = 0;
		m_iRear = iCopySize; // 꽉 찬 경우 Rear가 0이 될 수도 있음 (별도 처리 필요 시 추가)
	}
	else
	{
		m_iFront = 0;
		m_iRear = 0;
	}

	// 5. 새 포인터 연결
	m_pBuffer = pNewBuffer;
	m_iBufferSize = iSize;
}

void RingBuffer::ClearBuffer(void)
{
	m_iFront = 0;
	m_iRear = 0;
}

// -------- 상태 확인 --------

int RingBuffer::GetBufferSize(void)
{
	return m_iBufferSize;
}

int RingBuffer::GetUseSize(void)
{
	if (m_iRear >= m_iFront)
	{
		return m_iRear - m_iFront;
	}
	else
	{
		// Wrap Around 상태: (전체크기 - Front) + Rear
		return (m_iBufferSize - m_iFront) + m_iRear;
	}
}

int RingBuffer::GetFreeSize(void)
{
	// [중요] Full과 Empty를 구분하기 위해 1바이트는 사용하지 않음 (Dummy)
	// 따라서 전체 용량에서 1을 뺀 값에서 사용량을 뺌
	return (m_iBufferSize - 1) - GetUseSize();
}

// -------- 데이터 입출력 --------

int RingBuffer::Enqueue(const char* chpData, int iSize)
{
	if (iSize <= 0) return 0;

	// 1. 넣을 수 있는 공간 확인
	int iFreeSize = GetFreeSize();

	// 공간이 부족하면 남은 공간만큼만 넣음 (Partial Enqueue)
	if (iSize > iFreeSize)
	{
		iSize = iFreeSize;
	}

	if (iSize == 0) return 0;

	// 2. 복사 (Wrap Around 고려)
	int iDirectWrite = m_iBufferSize - m_iRear; // Rear부터 버퍼 끝까지 공간

	if (iSize <= iDirectWrite)
	{
		// 한 번에 쓰기 가능
		memcpy(&m_pBuffer[m_iRear], chpData, iSize);
	}
	else
	{
		// 두 번에 나누어 쓰기
		memcpy(&m_pBuffer[m_iRear], chpData, iDirectWrite); // 뒷부분 채우기
		memcpy(&m_pBuffer[0], chpData + iDirectWrite, iSize - iDirectWrite); // 앞부분 채우기
	}

	// 3. Rear 이동
	MoveRear(iSize);

	return iSize; // 실제 넣은 크기 리턴
}

int RingBuffer::Dequeue(char* chpDest, int iSize)
{
	if (iSize <= 0) return 0;

	// 1. 꺼낼 수 있는 데이터 확인
	int iUseSize = GetUseSize();

	// 요구량보다 데이터가 적으면 있는 만큼만 꺼냄
	if (iSize > iUseSize)
	{
		iSize = iUseSize;
	}

	if (iSize == 0) return 0;

	// 2. 데이터 복사 (Peek 로직과 동일)
	// chpDest가 nullptr이면 복사는 건너뛰고 포인터만 이동 (MoveFront와 유사 효과)
	if (chpDest != nullptr)
	{
		int iDirectRead = m_iBufferSize - m_iFront;

		if (iSize <= iDirectRead)
		{
			memcpy(chpDest, &m_pBuffer[m_iFront], iSize);
		}
		else
		{
			memcpy(chpDest, &m_pBuffer[m_iFront], iDirectRead);
			memcpy(chpDest + iDirectRead, &m_pBuffer[0], iSize - iDirectRead);
		}
	}

	// 3. Front 이동 (데이터 삭제 효과)
	MoveFront(iSize);

	return iSize;
}

int RingBuffer::Peek(char* chpDest, int iSize)
{
	if (iSize <= 0) return 0;

	int iUseSize = GetUseSize();
	if (iSize > iUseSize)
	{
		iSize = iUseSize;
	}

	if (iSize == 0) return 0;

	if (chpDest != nullptr)
	{
		int iDirectRead = m_iBufferSize - m_iFront;

		if (iSize <= iDirectRead)
		{
			memcpy(chpDest, &m_pBuffer[m_iFront], iSize);
		}
		else
		{
			memcpy(chpDest, &m_pBuffer[m_iFront], iDirectRead);
			memcpy(chpDest + iDirectRead, &m_pBuffer[0], iSize - iDirectRead);
		}
	}

	// 주의: Peek는 MoveFront를 호출하지 않음!
	return iSize;
}

void RingBuffer::MoveFront(int iSize)
{
	// 모듈러 연산 대신 조건문 사용 (성능상 유리할 수 있음)
	m_iFront += iSize;
	if (m_iFront >= m_iBufferSize)
	{
		m_iFront -= m_iBufferSize; // Wrap Around
	}
}

void RingBuffer::MoveRear(int iSize)
{
	m_iRear += iSize;
	if (m_iRear >= m_iBufferSize)
	{
		m_iRear -= m_iBufferSize; // Wrap Around
	}
}