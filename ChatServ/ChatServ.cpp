// ChatServ.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "build.h"
#include "fileutil.h"
#include "Config.h"
#include "Logger.h"
#include "Server.h"
#include <errno.h>
#include <signal.h>
#include "ServerConfig.h"
#include "IpFilter.h"
#include "strutil.h"


#define MAX_BIND_PORTS 1

int ServerConfig::max_channels = 0;
int ServerConfig::max_subscribers_per_channel = 0;
int ServerConfig::polling_timeout = 0;
int ServerConfig::polling_idles = 0;
int ServerConfig::channel_buffer_size = 0;
int ServerConfig::channel_timeout = 0;
int ServerConfig::channel_idles = 0;

Config *conf = NULL;
Server *serv = NULL;
IpFilter *ip_filter = NULL;
struct event_base *evbase = NULL;
struct evhttp *admin_http = NULL;
struct evhttp *front_http = NULL;
struct event *timer_event = NULL;
struct event *sigint_event = NULL;
struct event *sigterm_event = NULL;

void init(int argc, char **argv);

void welcome(){
	printf("chatserv %s,platform:win32\n", CHATSERV_VERSION);
	printf("Copyright (c) 2013-2014 ideawu.com\n");
	printf("\n");
}

void usage(int argc, char **argv){
	printf("Usage:you need to specify the configuration file,and run the application as follows:\n");
	//printf("%s    /path/to/chatserv.conf\n", argv[0]);
}

void signal_cb(evutil_socket_t sig, short events, void *user_data){
	event_base_loopbreak(evbase);
}

void poll_handler(struct evhttp_request *req, void *arg){
	serv->poll(req);
}

void iframe_handler(struct evhttp_request *req, void *arg){
	serv->iframe(req);
}

void stream_handler(struct evhttp_request *req, void *arg){
	serv->stream(req);
}

void ping_handler(struct evhttp_request *req, void *arg){
	serv->ping(req);
}

#define CHECK_AUTH() \
	do{ \
		evhttp_add_header(req->output_headers, "Server", "chatserv"); \
		bool pass = ip_filter->check_pass(req->remote_host); \
		if(!pass){ \
			log_info("admin deny %s:%d", req->remote_host, req->remote_port); \
			evhttp_add_header(req->output_headers, "Connection", "Close"); \
			evhttp_send_reply(req, 403, "Forbidden", NULL); \
			return; \
						} \
			}while(0)

void pub_handler(struct evhttp_request *req, void *arg){
	CHECK_AUTH();
	serv->pub(req, true);
}

void push_handler(struct evhttp_request *req, void *arg){
	CHECK_AUTH();
	serv->pub(req, false);
}

void sign_handler(struct evhttp_request *req, void *arg){
	CHECK_AUTH();
	serv->sign(req);
}

void close_handler(struct evhttp_request *req, void *arg){
	CHECK_AUTH();
	serv->close(req);
}

void clear_handler(struct evhttp_request *req, void *arg){
	CHECK_AUTH();
	serv->clear(req);
}

void info_handler(struct evhttp_request *req, void *arg){
	CHECK_AUTH();
	serv->info(req);
}

void check_handler(struct evhttp_request *req, void *arg){
	CHECK_AUTH();
	serv->check(req);
}

void psub_handler(struct evhttp_request *req, void *arg){
	CHECK_AUTH();
	serv->psub(req);
}
#undef CHECK_AUTH

void timer_cb(evutil_socket_t sig, short events, void *user_data){
	log_info("timer_event invoked");
	rand();
	serv->check_timeout();
}

void accept_error_cb(struct evconnlistener *lis, void *ptr){
	// do nothing
	log_info("accept_error_cb invoked");
}

int main(int argc, char* argv[])
{
	welcome();
	init(argc, argv);

	ServerConfig::max_channels = conf->get_num("front.max_channels");
	ServerConfig::max_subscribers_per_channel = conf->get_num("front.max_subscribers_per_channel");
	ServerConfig::channel_buffer_size = conf->get_num("front.channel_buffer_size");
	ServerConfig::channel_timeout = conf->get_num("front.channel_timeout");
	ServerConfig::polling_timeout = conf->get_num("front.polling_timeout");
	if (ServerConfig::polling_timeout <= 0){
		log_fatal("Invalid polling_timeout!");
		exit(0);
	}
	if (ServerConfig::channel_timeout <= 0){
		ServerConfig::channel_timeout = (int)(0.5 * ServerConfig::polling_timeout);
	}

	ServerConfig::polling_idles = ServerConfig::polling_timeout / CHANNEL_CHECK_INTERVAL;
	ServerConfig::channel_idles = ServerConfig::channel_timeout / CHANNEL_CHECK_INTERVAL;

	log_info("ServerConfig::max_channels =%d", ServerConfig::max_channels);
	log_info("ServerConfig::max_subscribers_per_channel=%d", ServerConfig::max_subscribers_per_channel);
	log_info("ServerConfig::polling_timeout=%d", ServerConfig::polling_timeout);
	log_info("ServerConfig::polling_idles =%d", ServerConfig::polling_idles);
	log_info("ServerConfig::channel_buffer_size=%d", ServerConfig::channel_buffer_size);
	log_info("ServerConfig::channel_timeout=%d", ServerConfig::channel_timeout);
	log_info("ServerConfig::channel_idles =%d", ServerConfig::channel_idles);
	serv = new Server();
	ip_filter = new IpFilter();


	{
		// /pub?cname=abc&content=hi
		// content must be json encoded string without leading quote and trailing quote
		// TODO: multi_pub
		evhttp_set_cb(admin_http, "/pub", pub_handler, NULL);
		// pub raw content(not json encoded)
		evhttp_set_cb(admin_http, "/push", push_handler, NULL);
		// 分配通道, 返回通道的id和token
		// /sign?cname=abc[&expires=60]
		// wait 60 seconds to expire before any sub
		evhttp_set_cb(admin_http, "/sign", sign_handler, NULL);
		// 销毁通道
		// /close?cname=abc
		evhttp_set_cb(admin_http, "/close", close_handler, NULL);
		// 销毁通道
		// /clear?cname=abc
		evhttp_set_cb(admin_http, "/clear", clear_handler, NULL);
		// 获取通道的信息
		// /info?[cname=abc], or TODO: /info?cname=a,b,c
		evhttp_set_cb(admin_http, "/info", info_handler, NULL);
		// 判断通道是否处于被订阅状态(所有订阅者断开连接一定时间后, 通道才转为空闲状态)
		// /check?cname=abc, or TODO: /check?cname=a,b,c
		evhttp_set_cb(admin_http, "/check", check_handler, NULL);

		// 订阅通道的状态变化信息, 如创建通道(第一个订阅者连接时), 关闭通道.
		// 通过 endless chunk 返回.
		evhttp_set_cb(admin_http, "/psub", psub_handler, NULL);

		std::string admin_ip;
		int admin_port = 0;
		parse_ip_port(conf->get_str("admin.listen"), &admin_ip, &admin_port);
		struct evhttp_bound_socket *handle;
		handle = evhttp_bind_socket_with_handle(admin_http, admin_ip.c_str(), admin_port);
		if (!handle){
			log_fatal("bind admin_port %d error! %s", admin_port, strerror(errno));
			exit(0);
		}
		log_info("admin server listen on %s:%d", admin_ip.c_str(), admin_port);

		struct evconnlistener *listener = evhttp_bound_socket_get_listener(handle);
		evconnlistener_set_error_cb(listener, accept_error_cb);
		// TODO: modify libevent, add evhttp_set_accept_cb()
	}

	// init admin ip_filter
	{
		Config *cc = (Config *)conf->get("admin");
		if (cc != NULL){
			std::vector<Config *> *children = &cc->children;
			std::vector<Config *>::iterator it;
			for (it = children->begin(); it != children->end(); it++){
				if ((*it)->key != "allow"){
					continue;
				}
				const char *ip = (*it)->str();
				log_info("    allow %s", ip);
				ip_filter->add_allow(ip);
			}
		}
	}

	{
		Config *cc = (Config *)conf->get("admin");
		if (cc != NULL){
			std::vector<Config *> *children = &cc->children;
			std::vector<Config *>::iterator it;
			for (it = children->begin(); it != children->end(); it++){
				if ((*it)->key != "deny"){
					continue;
				}
				const char *ip = (*it)->str();
				log_info("    deny %s", ip);
				ip_filter->add_deny(ip);
			}
		}
	}

	{
		// /sub?cname=abc&cb=jsonp&token=&seq=123&noop=123
		evhttp_set_cb(front_http, "/sub", poll_handler, NULL);
		evhttp_set_cb(front_http, "/poll", poll_handler, NULL);
		// forever iframe
		evhttp_set_cb(front_http, "/iframe", iframe_handler, NULL);
		// http endless chunk
		evhttp_set_cb(front_http, "/stream", stream_handler, NULL);
		// /ping?cb=jsonp
		evhttp_set_cb(front_http, "/ping", ping_handler, NULL);

		std::string front_ip;
		int front_port = 0;
		parse_ip_port(conf->get_str("front.listen"), &front_ip, &front_port);
		for (int i = 0; i < MAX_BIND_PORTS; i++){
			int port = front_port + i;

			struct evhttp_bound_socket *handle;
			handle = evhttp_bind_socket_with_handle(front_http, front_ip.c_str(), port);
			if (!handle){
				log_fatal("bind front_port %d error! %s", port, strerror(errno));
				exit(0);
			}
			log_info("front server listen on %s:%d", front_ip.c_str(), port);

			struct evconnlistener *listener = evhttp_bound_socket_get_listener(handle);
			evconnlistener_set_error_cb(listener, accept_error_cb);
		}

		std::string auth = conf->get_str("front.auth");
		log_info("    auth %s", auth.c_str());
		log_info("    max_channels %d", ServerConfig::max_channels);
		log_info("    max_subscribers_per_channel %d", ServerConfig::max_subscribers_per_channel);
		log_info("    channel_buffer_size %d", ServerConfig::channel_buffer_size);
		log_info("    channel_timeout %d", ServerConfig::channel_timeout);
		log_info("    polling_timeout %d", ServerConfig::polling_timeout);
		if (auth == "token"){
			serv->auth = Server::AUTH_TOKEN;
		}
	}

	//write_pidfile();
	log_info("chatserv started");
	event_base_dispatch(evbase);
	//remove_pidfile();

	event_free(timer_event);
	event_free(sigint_event);
	event_free(sigterm_event);
	evhttp_free(admin_http);
	evhttp_free(front_http);
	event_base_free(evbase);

	delete serv;
	delete conf;
	log_info("chatserv exit");

	return 0;
}

void init(int argc, char **argv)
{


	const char* conf_file = "chatserv.conf";
	if (!is_file(conf_file)){
		fprintf(stderr, "'%s' is not a file or not exists!\n", conf_file);
#ifdef _DEBUG
		system("pause");
#endif
		//exit(0);
	}

	conf = Config::load(conf_file);
	if (!conf){
		fprintf(stderr, "error loading conf file: '%s'\n", conf_file);
#ifdef _DEBUG
		system("pause");
#endif
		exit(0);
	}
	else
	{
		fprintf(stderr, "loading conf file success: '%s'\n", conf_file);
	}
	std::string log_output;
	int log_rotate_size = 0;
	{ // logger
		int log_level = Logger::get_level(conf->get_str("logger.level"));
		log_rotate_size = conf->get_num("logger.rotate.size");
		if (log_rotate_size < 1024 * 1024){
			log_rotate_size = 1024 * 1024;
		}
		log_output = conf->get_str("logger.output");
		if (log_output == ""){
			log_output = "stdout";
		}
		if (log_open(log_output.c_str(), log_level, true, log_rotate_size) == -1){
			fprintf(stderr, "error open log file: %s\n", log_output.c_str());
#ifdef _DEBUG
			system("pause");
#endif
			exit(0);
		}

		log_info("starting chatserv %s...", CHATSERV_VERSION);
		log_info("config file: %s", conf_file);
		log_info("log_level       : %s", conf->get_str("logger.level"));
		log_info("log_output      : %s", log_output.c_str());
		log_info("log_rotate_size : %d", log_rotate_size);

		WSADATA wsa = { 0 };
		int wsaStartup=WSAStartup(MAKEWORD(2, 2), &wsa);
		if (wsaStartup != 0)
		{
			log_error("failed to initialize WSAStartup:%d", wsaStartup);
			exit(0);
		}
		log_info("WSAStartup success :%d", wsaStartup);
		evbase = event_base_new();
		if (!evbase){
			fprintf(stderr, "create evbase error!\n");
			exit(0);
		}
		log_info("evbase created");
		admin_http = evhttp_new(evbase);
		if (!admin_http){
			fprintf(stderr, "create admin_http error!\n");
			exit(0);
		}
		log_info("admin_http created");
		front_http = evhttp_new(evbase);
		if (!front_http){
			fprintf(stderr, "create front_http error!\n");
			exit(0);
		}
		log_info("front_http created");

		sigint_event = evsignal_new(evbase, SIGINT, signal_cb, NULL);
		if (!sigint_event || event_add(sigint_event, NULL) < 0){
			fprintf(stderr, "Could not create/add a signal event!\n");
			exit(0);
		}
		log_info("sigint_event created/added");
		sigterm_event = evsignal_new(evbase, SIGTERM, signal_cb, NULL);
		if (!sigterm_event || event_add(sigterm_event, NULL) < 0){
			fprintf(stderr, "Could not create/add a signal event!\n");
			exit(0);
		}
		log_info("sigterm_event created/added");
		timer_event = event_new(evbase, -1, EV_PERSIST, timer_cb, NULL);
		{
			//用于检测无效的连接
			struct timeval tv;
			tv.tv_sec = CHANNEL_CHECK_INTERVAL;
			tv.tv_usec = 0;
			if (!timer_event || evtimer_add(timer_event, &tv) < 0){
				fprintf(stderr, "Could not create/add a timer event!\n");
				exit(0);
			}
			log_info("timer event added");
		}
	}

}