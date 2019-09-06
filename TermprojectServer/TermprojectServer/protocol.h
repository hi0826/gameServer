#pragma once

#define SERVER_PORT		3500

#define MAX_USER	10
#define NPC_START	10
#define MAX_NPC		10000
#define MAX_OBSTACLE 3000

#define BOARD_WIDTH 300
#define BOARD_HIGHT 300
#define RADIUS		7

#define CS_PACKET_UP 0
#define CS_PACKET_DOWN 1
#define CS_PACKET_LEFT 2
#define CS_PACKET_RIGHT 3
#define CS_PACKET_ATTACK 4

#define SC_PACKET_PUT_PLAYER 0
#define SC_PACKET_POS 1
#define SC_PACKET_REMOVE_OBJECT 2
#define SC_PACKET_PUT_NPC 3
#define SC_PACKET_INFO_PLAYER 4
#define SC_PACKET_UPDATE_PLAYER 5

#define LEVELUP 0
#define EXP 1

#pragma pack (push, 1)
struct sc_packet_pos {
	unsigned char size;
	unsigned char type;
	unsigned int id;
	unsigned int x;
	unsigned int y;
};

struct sc_packet_put_player {
	unsigned char size;
	unsigned char type;
	unsigned int id;
	unsigned int x;
	unsigned int y;
};

struct sc_packet_remove_player {
	unsigned char size;
	unsigned char type;
	unsigned int id;
};

struct sc_packet_player_info {
	unsigned char size;
	unsigned char type;
	unsigned char name[50];
	unsigned int id;
	unsigned int x;
	unsigned int y;
	unsigned int hp;
	unsigned int mp;
	unsigned int level;
	unsigned int exp;
	unsigned int sp;
};

struct sc_packet_update_player {
	unsigned char size;
	unsigned char type;
	unsigned char updateType;
};
struct cs_packet_up {
	unsigned char size;
	unsigned char type;
};

struct cs_packet_down {
	unsigned char size;
	unsigned char type;
};

struct cs_packet_left {
	unsigned char size;
	unsigned char type;
};

struct cs_packet_right {
	unsigned char size;
	unsigned char type;
};

struct cs_packet_id {
	unsigned char size;
	unsigned char id[50];
};
#pragma pack(pop)