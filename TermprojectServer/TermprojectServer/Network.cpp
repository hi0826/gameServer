#include "Network.h"

Network::Network()
{
	wcout.imbue(locale("korean"));
	WSADATA Wsadata;
	WSAStartup(MAKEWORD(2, 2), &Wsadata);

	listenSocket = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (listenSocket == INVALID_SOCKET) cout << "Error - Invalid socket" << endl;

	SOCKADDR_IN serverAddr;
	memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (::bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(SOCKADDR_IN))
		== SOCKET_ERROR) {
		cout << "Error - Fail bind" << endl;
		closesocket(listenSocket);
		exit(-1);
	}

	if (listen(listenSocket, 5) == SOCKET_ERROR) {
		cout << "Error - Fail listen" << endl;
		closesocket(listenSocket);
		WSACleanup();
		exit(-1);
	}

	hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);

	m_db = new DBManager();
}

Network::~Network()
{
	WSACleanup();
}

bool Network::Initialize()
{
	initMonsters();
	initObstacles();
	if (!m_db->ConnectDB()) {
		cout << "Don't connect DB" << endl;
		return false;
	}
	return true;
}

void Network::Run()
{
	vector<thread> worker_threads;
	thread accept_th;
	thread timer_th;
	accept_th = thread([this] { this->accept_thread(); });
	for (int i = 0; i < 4; ++i) {
		worker_threads.push_back(thread([this] {this->worker_thread(); }));
	}
	timer_th = thread([this] {this->timer_thread(); });

	for (auto &wth : worker_threads) wth.join();
	accept_th.join();
	timer_th.join();
}

void Network::accept_thread()
{
	cout << "Start Accept_Thread" << endl;

	while (true) {
		SOCKADDR_IN clientAddr;
		int addrLen = sizeof(SOCKADDR_IN);
		memset(&clientAddr, 0, addrLen);
		auto clientSocket = WSAAccept(listenSocket, (struct sockaddr*)&clientAddr, &addrLen, NULL, NULL);
		if (clientSocket == INVALID_SOCKET) {
			cout << "Error - Accept Failure" << endl;
			return;
		}

		int id = -1;
		for (int i = 0; i < MAX_USER; ++i) {
			if (false == clients[i].m_connected) {
				id = i;
				break;
			}
		}

		if (-1 == id) {
			cout << "user Full" << endl;
			closesocket(clientSocket);
			continue;
		}

		clients[id].id = id;
		clients[id].m_socket = clientSocket;
		clients[id].m_over_ex.m_todo = OP_LOGIN;
		clients[id].m_over_ex.m_wsa_buf.buf =
			(CHAR*)clients[id].m_over_ex.IOCPbuf;
		clients[id].m_over_ex.m_wsa_buf.len =
			sizeof(clients[id].m_over_ex.IOCPbuf);
		clients[id].m_prev_size = 0;
		clients[id].m_connected = true;
		clients[id].v_list.clear();
		clients[id].isid = true;

		CreateIoCompletionPort(
			reinterpret_cast<HANDLE>(clients[id].m_socket),
			hIOCP, id, 0);

		OverlappedRecv(id);

	}
}

void Network::worker_thread()
{
	cout << "Start Worker_Thread" << endl;

	while (true) {

		DWORD iosize;
		unsigned long key;
		OverEx *over;

		int ret = GetQueuedCompletionStatus(hIOCP, &iosize, (PULONG_PTR)&key,
			reinterpret_cast<WSAOVERLAPPED **>(&over), INFINITE);

		if (0 == ret) {
			int err_no = GetLastError();
			if (64 == err_no) {
				m_db->updateDB(clients[key]);
				ClientDisconnect(key);
			}
			else error_display("GQCS : ", err_no);
			continue;
		}

		if (0 == iosize) {
			ClientDisconnect(key);
			continue;
		}

		if (OP_RECV == over->m_todo) {
			// 패킷 재조립
			int rest_data = iosize;
			unsigned char *ptr = over->IOCPbuf;
			int packet_size = 0;
			if (0 != clients[key].m_prev_size)
				packet_size = clients[key].m_packet_buf[0];

			while (rest_data > 0) {
				if (0 == packet_size) packet_size = ptr[0];
				int need_size = packet_size - clients[key].m_prev_size;
				if (rest_data >= need_size) {
					// 패킷을 하나 조립할 수 있음
					memcpy(clients[key].m_packet_buf
						+ clients[key].m_prev_size,
						ptr, need_size);
					ProcessPacket(key, clients[key].m_packet_buf);
					packet_size = 0;
					clients[key].m_prev_size = 0;
					ptr += need_size;
					rest_data -= need_size;
				}
				else {
					// 패킷을 완성할 수 없으니 후일 기약하고 남은 데이터 저장
					memcpy(clients[key].m_packet_buf
						+ clients[key].m_prev_size,
						ptr, rest_data);
					ptr += rest_data;
					clients[key].m_prev_size += rest_data;
					rest_data = 0;
				}
			}
			OverlappedRecv(key);
		}
		else if (OP_SEND == over->m_todo) {
			delete over;
		}
		else if (OP_MOVE == over->m_todo)
		{
			moveNPC(key);
			bool player_exist = false;
			for (int i = 0; i < MAX_USER; ++i) {
				if (true == canSee(i, key)) {
					player_exist = true;
					break;
				}
			}
			if (player_exist)
				addTimer(key, job_move,
					high_resolution_clock::now() + 1s);
			else
				clients[key].m_moving = false;
			delete over;
		}
		else if (OP_LOGIN == over->m_todo) {
			// 패킷 재조립
			int rest_data = iosize;
			unsigned char *ptr = over->IOCPbuf;
			int packet_size = 0;
			if (0 != clients[key].m_prev_size)
				packet_size = clients[key].m_packet_buf[0];

			while (rest_data > 0) {
				if (0 == packet_size) packet_size = ptr[0];
				int need_size = packet_size - clients[key].m_prev_size + 1;
				if (rest_data >= need_size) {
					// 패킷을 하나 조립할 수 있음
					memcpy(clients[key].m_packet_buf
						+ clients[key].m_prev_size,
						ptr, need_size);
					if (m_db->searchDB(key, clients[key].m_packet_buf)) {
						copyClient(key, m_db->getClientObj());
						if (InitClient(key)) {
							packet_size = 0;
							clients[key].m_prev_size = 0;
							ptr += need_size;
							rest_data -= need_size;
							over->m_todo = OP_RECV;
							break;
						}
					}
					packet_size = 0;
					clients[key].m_prev_size = 0;
					ptr += need_size;
					rest_data -= need_size;
				}
				else {
					// 패킷을 완성할 수 없으니 후일 기약하고 남은 데이터 저장
					memcpy(clients[key].m_packet_buf
						+ clients[key].m_prev_size,
						ptr, rest_data);
					ptr += rest_data;
					clients[key].m_prev_size += rest_data;
					rest_data = 0;
				}
			}
			OverlappedRecv(key);
		}
		else {
			cout << "Unknown Worker Thread Job!\n";
			while (true);
		}
	}
}

void Network::timer_thread()
{
	cout << "Start Timer_Thread" << endl;
	while (true) {
		this_thread::sleep_for(1s);
		while (true) {
			tq_l.lock();
			if (true == timer_queue.empty()) {
				tq_l.unlock();
				break;
			}
			time_event ev = timer_queue.top();
			if (ev.wake_time <= high_resolution_clock::now()) {
				timer_queue.pop();
				tq_l.unlock();
				OverEx *ex_over = new OverEx;
				ex_over->m_todo = OP_MOVE;
				PostQueuedCompletionStatus(hIOCP, 1, ev.id, &
					ex_over->m_wsa_over);
			}
			else {
				tq_l.unlock();  break;
			}
		}
	}
}

void Network::send_packet(int id, void * packet)
{
	OverEx *ex = new OverEx;
	memcpy(ex->IOCPbuf, packet,
		reinterpret_cast<unsigned char *>(packet)[0]);
	ex->m_todo = OP_SEND;
	ex->m_wsa_buf.buf = (char *)ex->IOCPbuf;
	ex->m_wsa_buf.len = ex->IOCPbuf[0];
	ZeroMemory(&ex->m_wsa_over, sizeof(WSAOVERLAPPED));

	//cout << ex->m_wsa_buf.buf << endl;
	//printf("%d %d %d %d \n", ex->m_wsa_buf.buf[0], ex->m_wsa_buf.buf[1], ex->m_wsa_buf.buf[2], ex->m_wsa_buf.buf[3]);

	int ret = WSASend(clients[id].m_socket, &ex->m_wsa_buf, 1, NULL, 0,
		&ex->m_wsa_over, 0);
	//cout << clients[id].m_packet_buf << endl;
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			error_display("Accept:SEND: ", err_no);
	}
}

void Network::OverlappedRecv(int id)
{
	DWORD flags = 0;
	ZeroMemory(&clients[id].m_over_ex.m_wsa_over, sizeof(WSAOVERLAPPED));
	if (WSARecv(clients[id].m_socket,
		&clients[id].m_over_ex.m_wsa_buf, 1,
		NULL, &flags, &(clients[id].m_over_ex.m_wsa_over), 0))
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			printf("Error - IO pending Failure\n");
			return;
		}
	}
	else {
		cout << "Non Overlapped Recv return.\n";
		return;
	}
}

void Network::ClientDisconnect(int key)
{
	// 클라이언트가 종료해서 접속 끊어 졌다.
	sc_packet_remove_player packet;
	packet.id = key;
	packet.size = sizeof(packet);
	packet.type = SC_PACKET_REMOVE_OBJECT;
	for (int i = 0; i < MAX_USER; ++i) {
		if (false == clients[i].m_connected) continue;
		if (key == i) continue;
		clients[key].v_l.lock();
		unordered_set<int> vl = clients[key].v_list;
		clients[key].v_l.unlock();
		for (auto pl : vl) {
			clients[pl].v_l.lock();
			clients[pl].v_list.erase(key);
			clients[pl].v_l.unlock();
			send_packet(pl, &packet);
		}
		send_packet(i, &packet);
	}
	clients[key].m_connected = false;
}

void Network::error_display(const char * msg, int err_no)
{
	WCHAR *lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	cout << msg;
	wcout << L"에러 " << lpMsgBuf << endl;
	while (true);
	LocalFree(lpMsgBuf);
}

void Network::ProcessPacket(int id, unsigned char * packet)
{
	bool move = false, attack = false;
	int x = clients[id].x;
	int y = clients[id].y;

	switch (packet[1]) {
	case CS_PACKET_UP:
		y--;
		if (y <= 0) y = 0;
		if (!CollisionCheck(id, CS_PACKET_UP))
			move = true;
		break;

	case CS_PACKET_DOWN:
		y++;
		if (y >= BOARD_HIGHT) y = BOARD_HIGHT - 1;
		if (!CollisionCheck(id, CS_PACKET_DOWN))
			move = true;
		break;

	case CS_PACKET_LEFT:
		x--;
		if (x <= 0)	x = 0;
		if (!CollisionCheck(id, CS_PACKET_LEFT))
			move = true;
		break;

	case CS_PACKET_RIGHT:
		x++;
		if (x >= BOARD_WIDTH) x = BOARD_WIDTH - 1;
		if (!CollisionCheck(id, CS_PACKET_RIGHT))
			move = true;
		break;

	case CS_PACKET_ATTACK:
		cout << "attack" << endl;
		attack = true;
		break;
	default:
		cout << "Invalid Packet Type Error!\n";
		//while (true);
	}

	updateViewList(id);

	if (move)
		moveProcess(id, x, y);
	else if (attack)
		attackProcess(id, x, y);

	updatePlayer(id);
}

void Network::moveProcess(int id, int x, int y)
{
	clients[id].x = x;
	clients[id].y = y;

	sc_packet_pos pos_packet;
	pos_packet.id = id;
	pos_packet.size = sizeof(pos_packet);
	pos_packet.type = SC_PACKET_POS;
	pos_packet.x = x;
	pos_packet.y = y;

	send_packet(id, &pos_packet);

	clients[id].v_l.lock();
	for (auto pl : clients[id].v_list)
		if (isPlayer(pl))
			send_packet(pl, &pos_packet);

	clients[id].v_l.unlock();
	
}

void Network::attackProcess(int id, int x, int y)
{
	int distX, distY;
	unordered_set <int> old_vl;

	clients[id].v_l.lock();
	old_vl = clients[id].v_list;
	clients[id].v_l.unlock();

	for (auto pl : old_vl) {
		if (pl < MAX_USER) continue;
		if (pl >= MAX_NPC) continue;
		distX = clients[id].x - clients[pl].x;
		distY = clients[id].y - clients[pl].y;
		distX = abs(distX);
		distY = abs(distY);
		if (distX + distY >= 2) continue;
		clients[id].v_l.lock();
		clients[id].v_list.erase(pl);
		clients[id].v_l.unlock();
		send_remove_object_packet(id, pl);
		clients[pl].m_connected = false;
		clients[pl].m_moving = false;

		clients[id].exp += 10;
		sc_packet_update_player exp_packet;
		exp_packet.size = sizeof(exp_packet);
		exp_packet.type = SC_PACKET_UPDATE_PLAYER;
		exp_packet.updateType = EXP;
		send_packet(id, &exp_packet);
	}
}

bool Network::CollisionCheck(int id1, int type)
{
	int distY, distX;
	if (isPlayer(id1)) {
		clients[id1].v_l.lock();
		for (auto another : clients[id1].v_list) {
			if (!clients[another].m_connected) continue;
			if (type == CS_PACKET_UP) {
				distX = clients[id1].x - clients[another].x;
				distY = clients[id1].y - clients[another].y;
				if (distX == 0 && distY == 1) { 
					clients[id1].v_l.unlock();
					return true; 
				}
			}
			else if (type == CS_PACKET_DOWN) {
				distX = clients[id1].x - clients[another].x;
				distY = clients[id1].y - clients[another].y;
				if (distX == 0 && distY == -1) { 
					clients[id1].v_l.unlock(); 
					return true; 
				}
			}
			else if (type == CS_PACKET_LEFT) {
				distX = clients[id1].x - clients[another].x;
				distY = clients[id1].y - clients[another].y;
				if (distX == 1 && distY == 0) {
					clients[id1].v_l.unlock(); 
					return true; 
				}
			}
			else if (type == CS_PACKET_RIGHT) {
				distX = clients[id1].x - clients[another].x;
				distY = clients[id1].y - clients[another].y;
				if (distX == -1 && distY == 0) {
					clients[id1].v_l.unlock();
					return true; 
				}
			}

		}
		clients[id1].v_l.unlock();
	}

	return false;
}

bool Network::updateViewList(int id)
{
	// 시야에 있는 플레이어들 에게만 패킷을 보낸다.
	// 새로 시야에 들어오는 플레이어가 누구인지
	// 시야에서 사라진 플레이어가 누구인지
	unordered_set <int> old_vl, new_vl;

	clients[id].v_l.lock();
	old_vl = clients[id].v_list;
	clients[id].v_l.unlock();

	for (int i = 0; i < MAX_NPC + MAX_OBSTACLE; ++i) {
		if (false == clients[i].m_connected)
			continue;
		if (i == id)
			continue;
		if (true == canSee(id, i))
			new_vl.insert(i);
	}

	for (auto pl : new_vl) {
		if (0 == old_vl.count(pl)) {
			send_put_object_packet(id, pl);
			clients[id].v_l.lock();
			clients[id].v_list.insert(pl);
			clients[id].v_l.unlock();
			if (false == isPlayer(pl) && pl < MAX_NPC) {
				wakeupNPC(pl);
			}
		}

		if (false == isPlayer(pl)) continue;

		clients[pl].v_l.lock();
		if (0 != clients[pl].v_list.count(id)) {
			clients[pl].v_l.unlock();
		}
		else {
			clients[pl].v_list.insert(id);
			clients[pl].v_l.unlock();
			send_put_object_packet(pl, id);
		}
	}

	for (auto pl : old_vl) {
		if (0 == new_vl.count(pl)) {
			// 시야에서 사라진 플레이어
			clients[id].v_l.lock();
			clients[id].v_list.erase(pl);
			clients[id].v_l.unlock();
			send_remove_object_packet(id, pl);

			if (false == isPlayer(pl)) continue;

			clients[pl].v_l.lock();
			if (0 != clients[pl].v_list.count(id)) {
				clients[pl].v_list.erase(id);
				clients[pl].v_l.unlock();
				send_remove_object_packet(pl, id);
			}
			else {
				clients[pl].v_l.unlock();
			}
		}
	}
	return false;
}

void Network::updatePlayer(int id)
{
	sc_packet_update_player update_packet;

	if (clients[id].exp >= clients[id].level * 100) {
		clients[id].level++;
		clients[id].exp = 0;
		clients[id].hp += 10;
		clients[id].mp += 10;
		clients[id].sp += 1;
		update_packet.size = sizeof(update_packet);
		update_packet.type = SC_PACKET_UPDATE_PLAYER;
		update_packet.updateType = LEVELUP;
		send_packet(id, &update_packet);
	}
}

void Network::initMonsters()
{
	for (int i = NPC_START; i < MAX_NPC; ++i) {
		clients[i].id = i;
		clients[i].m_connected = true;
		clients[i].m_moving = false;
		clients[i].x = rand() % BOARD_WIDTH;
		clients[i].y = rand() % BOARD_HIGHT;
	}
}

void Network::initObstacles()
{
	for (int i = MAX_NPC; i < MAX_NPC + MAX_OBSTACLE; ++i) {
		clients[i].id = i;
		clients[i].m_connected = true;
		clients[i].m_moving = false;
		clients[i].x = rand() % BOARD_WIDTH;
		clients[i].y = rand() % BOARD_HIGHT;
	}
}

bool Network::canSee(int a, int b)
{
	if (!clients[a].m_connected || !clients[b].m_connected) return false;
	int dist = 0;
	dist += (clients[a].x - clients[b].x) * (clients[a].x - clients[b].x);
	dist += (clients[a].y - clients[b].y) * (clients[a].y - clients[b].y);

	if (dist <= RADIUS * RADIUS) return true;
	return false;
}

void Network::wakeupNPC(int id)
{
	if (true == clients[id].m_moving) return;
	clients[id].m_moving = true;
	addTimer(id, job_move, high_resolution_clock::now() + 1s);
}

void Network::addTimer(int id, int job_type, high_resolution_clock::time_point wt)
{
	tq_l.lock();
	timer_queue.push(time_event{ id, wt, job_type });
	tq_l.unlock();
}

void Network::send_put_object_packet(int client, int obj)
{
	if (obj >= NPC_START) {
		sc_packet_put_player packet;
		packet.id = obj;
		packet.size = sizeof(sc_packet_put_player);
		packet.type = SC_PACKET_PUT_NPC;
		packet.x = clients[obj].x;
		packet.y = clients[obj].y;
		send_packet(client, &packet);
	}

	else if (obj < MAX_USER) {
		sc_packet_player_info packet;
		packet.id = obj;
		packet.size = sizeof(sc_packet_player_info);
		packet.type = SC_PACKET_PUT_PLAYER;
		packet.x = clients[obj].x;
		packet.y = clients[obj].y;
		send_packet(client, &packet);
	}
}

void Network::send_remove_object_packet(int client, int obj)
{
	sc_packet_remove_player packet;
	packet.id = obj;
	packet.size = sizeof(packet);
	packet.type = SC_PACKET_REMOVE_OBJECT;
	send_packet(client, &packet);
}

void Network::moveNPC(int id)
{
	int x = clients[id].x;
	int y = clients[id].y;
	switch (rand() % 4) {
	case 0: if (x > 0) x--; break;
	case 1: if (x < BOARD_WIDTH - 1) x++; break;
	case 2: if (y > 0) y--; break;
	case 3: if (y < BOARD_HIGHT - 1) y++; break;
	}
	clients[id].x = x;
	clients[id].y = y;

	sc_packet_pos packet;
	packet.id = id;
	packet.size = sizeof(packet);
	packet.type = SC_PACKET_POS;
	packet.x = x;
	packet.y = y;

	for (int pl = 0; pl < MAX_USER; ++pl)
	{
		if (false == clients[pl].m_connected) continue;
		if (true == canSee(pl, id)) {
			clients[pl].v_l.lock();
			if (0 == clients[pl].v_list.count(id)) {
				clients[pl].v_list.insert(id);
				clients[pl].v_l.unlock();
				send_put_object_packet(pl, id);
				//cout << pl << ", " << id << endl;
			}
			else {
				clients[pl].v_l.unlock();
				send_packet(pl, &packet);
				//cout << "id : " << id << " move" << endl;
			}
		}
		else { // 시야에서 사라진 경우
			clients[pl].v_l.lock();
			if (0 == clients[pl].v_list.count(id)) {
				clients[pl].v_l.unlock();
			}
			else {
				clients[pl].v_list.erase(id);
				clients[pl].v_l.unlock();
				send_remove_object_packet(pl, id);
				//cout << "delete" << endl;
			}
		}
	}
}

bool Network::InitClient(int id)
{
	sc_packet_player_info packet;
	packet.size = sizeof(packet);
	strcpy((char*)packet.name, clients[id].name);
	packet.id = id;
	packet.type = SC_PACKET_PUT_PLAYER;
	packet.x = clients[id].x;
	packet.y = clients[id].y;
	packet.hp = clients[id].hp;
	packet.mp = clients[id].mp;
	packet.level = clients[id].level;
	packet.exp = clients[id].exp;
	packet.sp = clients[id].sp;

	sc_packet_put_player npcpacket;

	send_packet(id, &packet);

	// 신규 플레이어에게 다른 클라이언트들의 위치를 알려준다
	for (int i = 0; i < MAX_NPC + MAX_OBSTACLE; ++i) {
		if (false == clients[i].m_connected) continue;
		if (i == id) continue;
		if (false == canSee(id, i)) continue;

		if (false == isPlayer(i) && i < MAX_NPC) {
			wakeupNPC(i);
		}

		clients[id].v_l.lock();
		clients[id].v_list.insert(i);
		clients[id].v_l.unlock();

		if (i >= MAX_USER) {
			npcpacket.type = SC_PACKET_PUT_NPC;
			npcpacket.id = i;
			npcpacket.x = clients[i].x;
			npcpacket.y = clients[i].y;
			send_packet(id, &npcpacket);
		}
		else {
			packet.type = SC_PACKET_PUT_PLAYER;
			packet.id = i;
			packet.x = clients[i].x;
			packet.y = clients[i].y;
			send_packet(id, &packet);
		}
	}

	// 신규 플레이어의 접속을 다른 클라이언트에게 알려준다.
	for (int i = 0; i < MAX_USER; ++i) {
		if (false == clients[i].m_connected) continue;
		if (i == id) continue;
		if (false == canSee(i, id)) continue;
		clients[i].v_l.lock();
		clients[i].v_list.insert(id);
		clients[i].v_l.unlock();
		packet.id = id;
		packet.x = clients[id].x;
		packet.y = clients[id].y;

		send_packet(i, &packet);
	}

	return true;
}

void Network::copyClient(int key, ClientObject * pCli)
{
	strcpy(clients[key].name, pCli->name);
	clients[key].x = pCli->x;
	clients[key].y = pCli->y;
	clients[key].hp = pCli->hp;
	clients[key].mp = pCli->mp;
	clients[key].level = pCli->level;
	clients[key].exp = pCli->exp;
	clients[key].sp = pCli->sp;
	clients[key].skill_S = pCli->skill_S;
	clients[key].skill_D = pCli->skill_D;
}
