/*
 * TcpClient.cpp
 *
 *  Created on: 2 Mar 2023
 *      Author: elazarkin
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <smart_rtp/utils/TimeUtils.h>
#include <smart_rtp/tcp/TcpClient.h>

#include "internal_definitions.h"

TcpClient::TcpClient(const char *ip_addr, int port)
{
	sprintf(m_ip_addr, "%s", ip_addr);
	m_port = port;
	m_sockfd = -1;
	m_cache_size = 1024;
	m_cache = malloc(m_cache_size);
	m_connected_cache_size = 0;
	m_last_received_size = 0;
	m_last_received_time = 0;
	m_is_connection_alive = false;
	m_status_callback = NULL;
	m_ping_send_thread = NULL;
}

TcpClient::~TcpClient()
{
	// close connections, thread, free memory
	disconnect();
	free(m_cache);
}

ConnectionStatus TcpClient::connect(OnConnectionStatusCallback *)
{
	ConnectionCommand command_to_notify_cache_size;
	struct sockaddr_in serv_addr;
	if((m_sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return ConnectionStatus::SocketCreationFailed;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(m_port);

	if(inet_pton(AF_INET, m_ip_addr, &serv_addr.sin_addr) <= 0)
		return ConnectionStatus::BadServerAddress;

	if(::connect(m_sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		return ConnectionStatus::ClientConnectionToServerFailed;

	command_to_notify_cache_size.Command = ConnectionCommands::COMMAND_NOTIFY_CURRENT_MEMORY_SIZE;
	((size_t*) command_to_notify_cache_size.memory)[0] = m_cache_size;
	send(&command_to_notify_cache_size, sizeof(command_to_notify_cache_size));
	m_ping_send_thread = new std::thread(&TcpClient::ping_thread_fn, this);
	while(!m_is_connection_alive)
		usleep(100);
	if(m_status_callback != NULL)
		m_status_callback->OnConnected();

	return ConnectionStatus::Success;
}

bool TcpClient::send(void *data, size_t size)
{
	int read_size;
	static std::mutex send_mutex;
	// each send need have him receive, so it wrong create this function asyncronized!
	std::lock_guard<std::mutex> guard(send_mutex);

	::send(m_sockfd, data, size, 0);
	read_size = ::read(m_sockfd, m_cache, m_cache_size);
	m_last_received_time = getUseconds();
	if(read_size == sizeof(ConnectionCommand) && strcmp(((ConnectionCommand*)m_cache)->MAGIC_KEY, "SmartRtpConnectionCommand") == 0)
	{
		if(((ConnectionCommand*)m_cache)->Command == COMMAND_SET_MEMORY_SIZE_CAPACITY)
		{
			size_t new_size = ((size_t *)(((ConnectionCommand*)m_cache)->memory))[0];
			static const size_t MAX_ALLOCATION_SIZE = 100*1024*1024; // 100 MB

			if(new_size > MAX_ALLOCATION_SIZE)
			{
				fprintf(
					stderr, "Still not implemented BIG CACHE (more of %3.2f MB) size! This is critical error that close server!\n",
					MAX_ALLOCATION_SIZE/(1024.0f*1024)
				);
				exit(137);
			}
			if(new_size > m_cache_size)
			{
				printf("update cache memory from %zu to %zu\n", m_cache_size, new_size);
				if(realloc(m_cache, new_size) == NULL)
				{
					fprintf(stderr, "Have serious problem - FAIL REALLOC MEMORY!\n");
					exit(137);
				}
				m_cache_size = new_size;
			}
			// after system command we still wait to actual data
			read_size = ::read(m_sockfd, m_cache, m_cache_size);
		}
		else if(((ConnectionCommand*)m_cache)->Command == COMMAND_NOTIFY_CURRENT_MEMORY_SIZE)
			m_connected_cache_size = ((size_t *)(((ConnectionCommand*)m_cache)->memory))[0];
		else if(((ConnectionCommand*)m_cache)->Command == COMMAND_ALIVE)
		{
			// Do nothing in this case - this is repeating ping thread
			// TODO add disconnection thread
			// that check last receive and kill the client someself if connection failed
			return true;
		}
		// TODO add server fail exit case!
	}
	m_last_received_size = read_size;
	if(m_status_callback != NULL)
		m_status_callback->OnDataReceived(data, m_last_received_size);
	return true;
}

const void *TcpClient::received_data(){return m_cache;}
size_t TcpClient::receive_data_size(){return m_last_received_size;}
bool TcpClient::is_connected(){return m_is_connection_alive;}

void TcpClient::disconnect()
{
	// close socket, stop ping thread
	if(m_status_callback != NULL)
		m_status_callback->OnDisconnected();
	m_is_connection_alive = false;
	if(m_ping_send_thread != NULL)
	{
		if(m_ping_send_thread->joinable())
			m_ping_send_thread->join();
		delete(m_ping_send_thread);
	}
	if(m_sockfd >= 0)
		::close(m_sockfd);
}

void TcpClient::ping_thread_fn()
{
	ConnectionCommand command;
	command.Command = ConnectionCommands::COMMAND_ALIVE;
	m_is_connection_alive = true;
	while(m_is_connection_alive)
	{
		send(&command, sizeof(command));
		sleep(1);
	}
}
