#pragma once

class RingBuffer {
public:
	// -------- 생성자 및 소멸자 --------
	RingBuffer(void);
	RingBuffer(int iBufferSize);
	virtual ~RingBuffer(void);

	// -------- 초기화 및 리사이징 --------
	/// <summary>
	/// 버퍼 크기 동적 변경
	/// 기존 데이터 유지, 새로운 크기로 재배치
	/// </summary>
	/// <param name="iSize">보통 현재 사이즈 * 2~3</param>
	void Resize(int iSize);

	/// <summary>
	/// 버퍼를 깨끗이 비우기 (포인터 초기화)
	/// </summary>
	/// <param name=""></param>
	void ClearBuffer(void);

	// -------- 상태 확인 --------

	/// <summary>
	/// 전체 할당된 버퍼 크기
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	int GetBufferSize(void);

	/// <summary>
	/// 현재 사용 중인 데이터 크기
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	int GetUseSize(void);

	/// <summary>
	/// 남은 여유 공간 크기
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	int GetFreeSize(void);

	// -------- 데이터 입출력 --------
	/// <summary>
	/// 데이터 넣기
	/// </summary>
	/// <param name="chpDest">데이터 포인터</param>
	/// <param name="iSize">크기</param>
	/// <returns>수신 시 실제 넣은 크기, iSize보다 작을 수 있음. 부분삽입</returns>
	int Enqueue(const char* chpData, int iSize);
	/// <summary>
	/// 데이터 꺼내기
	/// </summary>
	/// <param name="chpDest">받을 데이터를 저장할 포인터</param>
	/// <param name="iSize">크기</param>
	/// <returns>실제 꺼낸 크기. 꺼낸 만큼 버퍼에서 삭제됨</returns>
	int Dequeue(char* chpDest, int iSize);
	/// <summary>
	/// 데이터 들여다보기 (삭제 안함)
	/// </summary>
	/// <param name="chpDest">받을 데이터를 저장할 포인터</param>
	/// <param name="iSize">크기</param>
	/// <returns>삭제하지 않고 복사만 해옴. 주로 헤더 확인용</returns>
	int Peek(char* chpDest, int iSize);
	/// <summary>
	/// 
	/// </summary>
	/// <param name="iSize"></param>
	void MoveFront(int iSize);
	/// <summary>
	/// 
	/// </summary>
	/// <param name="iSize"></param>
	void MoveRear(int iSize);

private:
	// -------- 내부 멤버 변수 --------
	char* m_pBuffer; // 실제 메모리 포인터
	int m_iBufferSize; //  전체 버퍼 크기

	int m_iFront; // 데이터 시작 위치 (데이터 꺼내는 위치)
	int m_iRear;  // 데이터 끝 위치 (데이터 넣는 위치)
};