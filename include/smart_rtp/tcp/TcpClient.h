/*
 * TcpClient.h
 *
 *  Created on: 2 Mar 2023
 *      Author: elazarkin
 */

#ifndef INCLUDE_SMART_RTP_TCP_TCPCLIENT_H_
#define INCLUDE_SMART_RTP_TCP_TCPCLIENT_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <mutex>
#include <thread>
#include "definitions.h"

class TcpClient
{
	class OnConnectionStatusCallback
	{
	public:
		virtual ~OnConnectionStatusCallback()=0;
		virtual void OnConnected();
		virtual void OnDisconnected();
		virtual void OnDataReceived(void *data, size_t);
	};
private:
	uint32_t m_sockfd;
	char m_ip_addr[32];
	uint16_t m_port;
	void *m_cache;
	size_t m_cache_size, m_connected_cache_size;
	size_t m_last_received_size;
	std::mutex m_cache_mutex;
	std::thread *m_ping_send_thread;
	uint64_t m_last_received_time;
	OnConnectionStatusCallback *m_status_callback;
	bool m_is_connection_alive;
public:
	TcpClient(const char *ip_addr, int port);
	virtual ~TcpClient();

	ConnectionStatus connect(OnConnectionStatusCallback *);
	void disconnect();
	bool send(void *data, size_t size);
	const void *received_data();
	size_t receive_data_size();
	bool is_connected();
	void ping_thread_fn();

};

#endif /* INCLUDE_SMART_RTP_TCP_TCPCLIENT_H_ */
