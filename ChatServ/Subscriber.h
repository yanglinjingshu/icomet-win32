#pragma once
#include <string>
class Server;
class Channel;

class Subscriber{
public:
	enum Type{
		POLL = 0,
		STREAM = 1,
		IFRAME = 2
	};
public:
	Subscriber();
	~Subscriber();

	Subscriber *prev;
	Subscriber *next;
	Channel *channel;
	std::string callback;
	int type;
	int idle;

	int seq_next;
	int seq_noop;
	struct evhttp_request *req;

	void start();
	void close();
	void send_chunk(int seq, const char* type, const char* content);

	void noop();
	void send_old_msgs();

	static void send_error_reply(int sub_type, struct evhttp_request *req, const char *cb, const std::string &cname, const char *type, const char *content);
};
