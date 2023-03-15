//
// Created by elazarkin on 24/02/2023.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>

#include <smart_rtp/tcp/TcpServer.h>
#include <smart_rtp/utils/TimeUtils.h>

#include "internal_definitions.h"

TcpServer::TcpServer(int tcp_port)
{
	m_self_sockfd = -1;
	m_port = (uint16_t)tcp_port;
	m_callback = NULL;
	m_flag_server_up = false;
	m_ping_thread = NULL;
}

TcpServer::~TcpServer()
{
	// TODO
	// close - stop all threads (clients + ping)
	close();
	// clean all memories
	printf("server destroyed success!\n");
}

void TcpServer::ping_checker_thread()
{
	printf("ping thread started!\n");
	while(m_flag_server_up)
	{
		uint64_t curr_time = getUseconds();
		static const uint64_t MAX_TIME_DIFF_FOR_DISCONNECTION = 3000000L;
		if( m_clients.size() > 0)
		{
			std::lock_guard<std::mutex> guard(m_clients_vector_change_mutex);
			for(int i = 0; i < m_clients.size();)
			{
				if(curr_time - m_clients[i]->last_receive_time() > MAX_TIME_DIFF_FOR_DISCONNECTION)
				{
					Client *ptr = m_clients[i];
					m_clients.erase(m_clients.begin() + i);
					delete ptr;
				}
				else i++;
			}
		}
		sleep(1);
	}
	printf("ping thread finish!\n");
}

ConnectionStatus TcpServer::start(OnReceiveCallback *callback)
{
	int32_t connection_sockfd;
    socklen_t len;
    struct sockaddr_in servaddr, cli;
    struct timeval timeout;
    ConnectionCommand command_to_notify_cache_size;

    m_callback = callback;

    m_self_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_self_sockfd == -1)
        return SocketCreationFailed;

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(m_port);

    if ((bind(m_self_sockfd, (sockaddr*)&servaddr, sizeof(servaddr))) != 0)
        return SocketBindPortFailed;

    if ((listen(m_self_sockfd, 5)) != 0)
        return SocketListenFailed;

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if (setsockopt (m_self_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0)
		fprintf(stderr, "Warning: setsockopt SO_RCVTIMEO failed\n");
	if (setsockopt (m_self_sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0)
		fprintf(stderr, "Warning: setsockopt failed\n");

    len = sizeof(cli);
    printf("server wait for connection...\n");

    m_flag_server_up = true;
    m_ping_thread = new std::thread(&TcpServer::ping_checker_thread, this);

    while(m_flag_server_up)
    {
		connection_sockfd = accept(m_self_sockfd, (sockaddr*)&cli, &len);
		if (connection_sockfd < 0)
			usleep(100);
		else
		{
			m_callback_call_mutex.lock();
			callback->onConnected(connection_sockfd);
			m_callback_call_mutex.unlock();

			m_clients_vector_change_mutex.lock();
			m_clients.push_back(new Client(this, connection_sockfd, (sockaddr*)&cli, callback));
			m_clients_vector_change_mutex.unlock();
		}
    }

    return ConnectionStatus::FinishSuccess;
}

// TODO not used!
void TcpServer::onClientReceiveCallbackWrapper(Client *client, void *data, uint32_t size)
{
	std::lock_guard<std::mutex> mutex_guard(m_callback_call_mutex);
	m_callback->onRequestToRepeat(client, data, size);
}

void TcpServer::send(Client *client, void *data, uint32_t size)
{
#ifdef DEBUG_PRINTS
	printf("%zu try send %zu bytes\n", client->id(), size);
#endif
	if(size > client->get_connection_cache_size())
	{
		ConnectionCommand command;
		command.Command = COMMAND_SET_MEMORY_SIZE_CAPACITY;
		((uint32_t *) command.memory)[0] = size + 1024;
		printf(
			"%zu: %zu bytes bigger for receive on it on other side (have %zu bytes cache), send command increase this to %zu\n",
			client->id(), size, client->get_connection_cache_size(), ((uint32_t *) command.memory)[0]
		);
		send_wrapper(client->id(), &command, sizeof(command));
		client->set_connection_cache_size(((uint32_t *) command.memory)[0]);
	}
	send_wrapper(client->id(), data, size);
}

void TcpServer::close()
{
	m_clients_vector_change_mutex.lock();
	//  * disconnect all clients (and stop they threads)
	while(m_clients.size() > 0)
	{
		Client *ptr = m_clients.begin()[0];
		m_clients.erase(m_clients.begin());
		delete ptr;
	}
	m_clients_vector_change_mutex.unlock();
	if(m_self_sockfd > 0)
		::close(m_self_sockfd);
	m_flag_server_up = false;

	//  * close all threads (ping)
	if(m_ping_thread->joinable())
		m_ping_thread->join();
	//  * close main sockfd and finish run function
	 printf("server closed success!\n");
}

uint32_t TcpServer::get_used_cache_size()
{
	std::lock_guard<std::mutex> guard(m_clients_vector_change_mutex);
	uint32_t cache_size = 0;
	for(Client *c:m_clients)
		cache_size += c->cache_size();

	return cache_size;
}

void TcpServer::print_status_name(const ConnectionStatus &status)
{
	switch(status)
	{
	case ConnectionStatus::FinishSuccess:
		printf("FinishSuccess");
		break;
	case ConnectionStatus::SocketCreationFailed:
		printf("SocketCreationFailed");
		break;
	case ConnectionStatus::SocketBindPortFailed:
		printf("SocketBindPortFailed");
		break;
	case ConnectionStatus::SocketListenFailed:
		printf("SocketListenFailed");
		break;
	case ConnectionStatus::ServerSocketConnectionTimeout:
		printf("ServerSocketConnectionTimeout");
		break;
	case ConnectionStatus::ClientConnectionToServerFailed:
		printf("ClientConnectionToServerFailed");
		break;
	case ConnectionStatus::ConnectionInitBadProtocol:
		printf("ConnectionInitBadProtocol");
		break;
	}
}

TcpServer::Client::Client(TcpServer *server_ptr, int32_t connection_sockfd, sockaddr *addr, OnReceiveCallback *on_receive_callback)
{
	int thread_fail_counter = 100;
	m_server_ptr = server_ptr;
	m_client_id = (uint64_t) connection_sockfd;
	m_listener = new std::thread(&TcpServer::Client::listener_fn, this);
	m_listener_alive = false;
	m_cache_size = 1024;
	m_cache = malloc(m_cache_size);
	m_connected_cache_size = 0;
	m_on_receive_callback = on_receive_callback;
	m_last_received_time = 0;
	while(!m_listener_alive && thread_fail_counter-->0)
		usleep(100);

	if(thread_fail_counter <= 0)
		throw std::runtime_error("some problem start listener thread of client!");
}

TcpServer::Client::~Client()
{
	m_listener_alive = false;
	::shutdown(m_client_id, 2);
	::close(m_client_id);
	m_client_id = -1;
	if(m_listener->joinable())
		m_listener->join();
	delete m_listener;
	if(m_cache != NULL)
		free(m_cache);
}

void TcpServer::Client::listener_fn()
{
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if (setsockopt (m_client_id, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) < 0)
		fprintf(stderr, "Warning: setsockopt SO_RCVTIMEO failed\n");
	if (setsockopt (m_client_id, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout) < 0)
		fprintf(stderr, "Warning: setsockopt failed\n");

	m_listener_alive = true;
	while(m_listener_alive)
	{
		uint32_t read_size = receive_wrapper(m_client_id, m_cache, m_cache_size);

		if(read_size > 0)
		{
			set_last_receive_time(getUseconds());
			if(read_size == sizeof(ConnectionCommand) && strcmp(((ConnectionCommand*)m_cache)->MAGIC_KEY, "SmartRtpConnectionCommand") == 0)
			{
				ConnectionCommand repeat_command;
				if(((ConnectionCommand*)m_cache)->Command == COMMAND_SET_MEMORY_SIZE_CAPACITY)
				{
					uint32_t new_size = ((uint32_t *)(((ConnectionCommand*)m_cache)->memory))[0];
					static const uint32_t MAX_ALLOCATION_SIZE = 100*1024*1024; // 100 MB

					printf("%zu: have COMMAND_SET_MEMORY_SIZE_CAPACITY to %zu B\n", m_client_id, new_size);

					if(new_size > MAX_ALLOCATION_SIZE)
					{
						fprintf(
							stderr, "Still not implemented BIG CACHE (more of %3.2f MB) size! This is critical error that close server!\n",
							MAX_ALLOCATION_SIZE/(1024.0f*1024)
						);
						repeat_command.Command = ConnectionCommands::COMMAND_CRITICAL_ERROR_EXIT;
						m_server_ptr->send(this, &repeat_command, sizeof(repeat_command));
						exit(137);
					}

					if(new_size > m_cache_size)
					{
						printf("%zu: update cache memory from %zu to %zu\n", m_client_id, m_cache_size, new_size);
						free(m_cache);
						if((m_cache = malloc(new_size)) == NULL)
						{
							fprintf(stderr, "Have serious problem - FAIL REALLOC MEMORY!\n");
							repeat_command.Command = ConnectionCommands::COMMAND_CRITICAL_ERROR_EXIT;
							m_server_ptr->send(this, &repeat_command, sizeof(repeat_command));
							exit(137);
						}
						m_cache_size = new_size;
					}
					repeat_command.Command = ConnectionCommands::COMMAND_NOTIFY_CURRENT_MEMORY_SIZE;
					((uint32_t*)repeat_command.memory)[0] = m_cache_size;
					m_server_ptr->send(this, &repeat_command, sizeof(repeat_command));
				}
				else if(((ConnectionCommand*)m_cache)->Command == COMMAND_NOTIFY_CURRENT_MEMORY_SIZE)
				{
					printf(
							"%zu: get notification about memory size of other side %zu\n",
							m_client_id,
							((uint32_t *)(((ConnectionCommand*)m_cache)->memory))[0]
					);
					m_connected_cache_size = ((uint32_t *)(((ConnectionCommand*)m_cache)->memory))[0];
					memcpy(&repeat_command, m_cache, sizeof(repeat_command));
					((uint32_t *)(repeat_command.memory))[0];
					m_server_ptr->send(this, &repeat_command, sizeof(repeat_command));
				}
				else if(((ConnectionCommand*)m_cache)->Command == COMMAND_ALIVE)
				{
					memcpy(&repeat_command, m_cache, sizeof(repeat_command));
					m_server_ptr->send(this, &repeat_command, sizeof(repeat_command));
				}
				else
				{
					printf("%zu: COMMAND_NOT_FOUND!\n", m_client_id);
					repeat_command.Command = ConnectionCommands::COMMAND_NOT_FOUND;
					m_server_ptr->send(this, &repeat_command, sizeof(repeat_command));
				}
			}
			else
			{
				// TODO care about mechanism if request not repited - warn about it and send some system notification
				//  to continue stable communication
				m_on_receive_callback->onRequestToRepeat(this, m_cache, read_size);
			}
		}
		else if(read_size == 0)
		{
			// TODO implement this disconnected trigger!
			usleep(100);
		}
		else
			usleep(100);
	}
}

uint32_t TcpServer::Client::cache_size()
{
	return m_cache_size;
}

uint32_t TcpServer::Client::get_connection_cache_size()
{
	return m_connected_cache_size;
}

void TcpServer::Client::set_connection_cache_size(uint32_t size)
{
	m_connected_cache_size = size;
}

uint64_t TcpServer::Client::id(){ return m_client_id;}
uint64_t TcpServer::Client::last_receive_time(){return m_last_received_time;}
void TcpServer::Client::set_last_receive_time(uint64_t time){m_last_received_time = time;}
