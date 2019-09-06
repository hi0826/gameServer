#pragma once
#include "includes.h"
#include "protocol.h"
#include "Objects.h"
#include "DBManager.h"
;

#define OP_RECV 0
#define OP_SEND 1
#define OP_MOVE 2
#define OP_LOGIN 3
#define OP_LOGIN_OK 4

class Network 
{
public:
	Network();
	~Network();
	
	bool Initialize();
	void Run();

	void accept_thread();
	void worker_thread();
	void timer_thread();

	void send_packet(int id, void* packet);
	void OverlappedRecv(int id);

	void ClientDisconnect(int key);

	void error_display(const char *msg, int err_no);

	void ProcessPacket(int id, unsigned char *packet);
	void moveProcess(int id, int x, int y);
	void attackProcess(int id, int x, int y);
	bool CollisionCheck(int id1, int type);

	bool updateViewList(int id);
	void updatePlayer(int id);

	void initMonsters();
	void initObstacles();

	bool canSee(int a, int b);
	bool isPlayer(int id) { return id < MAX_USER; }

	void wakeupNPC(int id);
	void addTimer(int id, int job_type, high_resolution_clock::time_point wt);
	void moveNPC(int id);

	void send_put_object_packet(int client, int obj);
	void send_remove_object_packet(int client, int obj);

	
	bool InitClient(int id);
	void copyClient(int key, ClientObject* pCli);

private : 
	// handle IOCP
	HANDLE hIOCP;

	SOCKET listenSocket;

	ClientObject clients[MAX_NPC + MAX_OBSTACLE];
	//ClientObject monsters[MAX_NPC];
	const int job_move = 0;
	const int job_respawn = 1;

	priority_queue <time_event, vector<time_event>, event_comp> timer_queue;
	mutex tq_l;

	DBManager* m_db;
	
};