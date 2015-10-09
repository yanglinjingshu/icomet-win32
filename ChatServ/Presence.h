#ifndef __chatserv__Persence__
#define __chatserv__Persence__

class Server;

enum PresenceType{
	PresenceOffline = 0,
	PresenceOnline = 1
};

class PresenceSubscriber
{
public:
	PresenceSubscriber *prev;
	PresenceSubscriber *next;

	Server *serv;
	struct evhttp_request *req;
};

#endif
