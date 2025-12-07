#pragma once

// ★ 핵심: INetworkHandler를 상속받음
class CGameServer : public INetworkHandler {
public:
	CGameServer();
	~CGameServer();

	// 1. 네트워크 매니저 연결 (패킷 보내기 & 유저 목록 조회용)
	void SetNetworkManager(CNetworkManager* net) { m_network = net; }

	// 2. [주기적 로직] 메인 루프에서 매번 호출 (이동 연산 등)
	void Update(double dt);

	// 3. [반응형 로직] 패킷이 오면 호출됨 (Switch문 여기 있음)
	virtual void HandlePacket(Session* session, CPacket& packet) override;

private:
	// 내부 로직 함수들 (기존 코드 이동)
	void MoveCharacter(Session* session, double dt);
	void ProcAttack(Session* attacker, int attackType);
	bool CheckGlobalCooldown(Session* session, int attackType);

private:
	CNetworkManager* m_network; // 네트워크 기능 사용을 위한 포인터
};