#pragma once
#include "includes.h"

#define MAX_BUFFER 1024
#define NAME_LEN 50


struct OverEx {
	WSAOVERLAPPED	m_wsa_over;
	WSABUF			m_wsa_buf;
	unsigned char	IOCPbuf[MAX_BUFFER];
	unsigned char	m_todo;
};

struct Object {
	int id;
	bool m_connected;
	bool m_moving;
	char name[NAME_LEN];
	int x, y;
	int hp, mp;
};


struct ClientObject : public Object {
	int level;
	int exp;
	int sp;
	int skill_S;
	int skill_D;
	bool isid;

	unordered_set <int>	v_list;
	mutex v_l;

	SOCKET m_socket;
	OverEx m_over_ex;
	unsigned char m_packet_buf[MAX_BUFFER];
	unsigned short  m_prev_size;
};

class time_event {
public :
	int id;
	high_resolution_clock::time_point wake_time;
	int job;
};

class event_comp{
public :
	event_comp() {}
	bool operator() (const time_event lhs, const time_event rhs) const
	{
		return (lhs.wake_time > rhs.wake_time);
	}
};