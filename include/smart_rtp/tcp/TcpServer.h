//
// Created by elazarkin on 24/02/2023.
//

#ifndef MAFHIDYONIM_TCPSERVER_H
#define MAFHIDYONIM_TCPSERVER_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>

#include <mutex>
#include <thread>
#include <vector>

#include "definitions.h"

class TcpServer
{
public:
	// TODO - solve this dependency between OnReceiveCallback and Client
	class OnReceiveCallback;

	class Client
	{
	private:
		TcpServer *m_server_ptr;
		uint64_t m_client_id;
		sockaddr m_addr;
		std::thread *m_listener;
		OnReceiveCallback *m_on_receive_callback;
		void *m_cache;
		size_t m_cache_size, m_connected_cache_size;
		bool m_listener_alive;
		uint64_t m_last_received_time;
	public:
		Client(TcpServer *server_ptr, int32_t connection_sockfd, sockaddr *addr, OnReceiveCallback *);
		virtual ~Client();
		uint64_t id();
		size_t cache_size();
		size_t get_connection_cache_size();
		void set_connection_cache_size(size_t);
		uint64_t last_receive_time();
	private:
		void listener_fn();
	protected:
		void set_last_receive_time(uint64_t);
	};

	class OnReceiveCallback
	{
	public:
		virtual ~OnReceiveCallback() {};
		virtual void onConnected(uint64_t client_id) = 0;
		virtual void onDisconnected(uint64_t client_id) = 0;
		virtual void onRequestToRepeat(Client *client, void *data, size_t size) = 0;
	};
private:
	int32_t m_self_sockfd;
	uint16_t m_port;
	bool m_flag_server_up;

	std::mutex m_callback_call_mutex;
	OnReceiveCallback *m_callback;

	std::mutex m_clients_vector_change_mutex;
	std::vector<Client *> m_clients;

	std::thread *m_ping_thread;
public:
    TcpServer(int port);
    virtual ~TcpServer();
    ConnectionStatus start(OnReceiveCallback *callback);
    void send(Client *client, void *data, size_t size);
    void close();

    static void print_status_name(const ConnectionStatus &status);


private:
    void onClientReceiveCallbackWrapper(Client *client, void *data, size_t size);
    void ping_checker_thread();
    size_t get_used_cache_size();
};


#endif //MAFHIDYONIM_TCPSERVER_H
