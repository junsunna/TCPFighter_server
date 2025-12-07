#pragma once

class INetworkHandler {
public:
	// 순수 가상 함수: 상속받는 쪽에서 반드시 구현해야 함 (Switch문 분기 처리)
	virtual void HandlePacket(Session* session, CPacket& packet) = 0;
};

class CNetworkManager {
public:
	CNetworkManager();
	~CNetworkManager();

	bool Init(int port, INetworkHandler* handler);
	
	void CleanUp();

	void NetworkProc(); 
	
	// 패킷 전송 함수 (이제 CPacket을 받음)
	void SendPacket(Session* session, CPacket* packet);
	void BroadcastPacket(Session* excludeSession, CPacket* packet);

	// 세션 관리
	std::unordered_map<SOCKET, Session*>& GetClients() { return m_clients; }
	
	void Disconnect(SOCKET sock);

private:
	void AcceptNewClient();
	void FlushSendBuffer(Session* session);

private:
	SOCKET m_listenSock;
	
	std::unordered_map<SOCKET, Session*> m_clients;

	int m_idGenerator = 1;

	// Select 모델용 셋
	FD_SET m_readSet;

	INetworkHandler* m_handler;
};