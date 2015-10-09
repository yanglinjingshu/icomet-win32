#pragma once
#include "build.h"
#include <string>
#include <vector>
#include "listutil.h"

#define CHANNEL_MSG_LIST_SIZE	10

class Server;
class Subscriber;


class Channel{
private:
public:
	inline static bool SEQ_GT(int a, int b){
		return a - b > 0;
	}
	inline static bool SEQ_GE(int a, int b){
		return a - b >= 0;
	}
	inline static bool SEQ_LT(int a, int b){
		return a - b < 0;
	}
	inline static bool SEQ_LE(int a, int b){
		return a - b <= 0;
	}
public:
	Channel *prev;
	Channel *next;

	Server *serv;
	LinkedList<Subscriber *> subs;

	int idle;
	int seq_next;
	std::string name;
	std::string token;

	std::vector<std::string> msg_list;

	Channel();
	~Channel();

	inline int msg_seq_min() const{
		if (msg_list.empty()){
			return seq_next;
		}
		else if (msg_list.size() > CHANNEL_MSG_LIST_SIZE){
			return seq_next - CHANNEL_MSG_LIST_SIZE;
		}
		else{
			return seq_next - msg_list.size();
		}
	}
	void create_token();

	void add_subscriber(Subscriber *sub);
	void del_subscriber(Subscriber *sub);
	void send(const char *type, const char *content, bool encoded = true);
	void clear();
	void close();
};

