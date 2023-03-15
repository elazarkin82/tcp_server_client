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
#include <fcntl.h>
#include <time.h>

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
	ConnectionStatus ret_status = ConnectionStatus::Success;
	ConnectionCommand command_to_notify_cache_size;
	long arg;
	fd_set myset;
	int valopt, res;
	struct timeval timeout;
	struct sockaddr_in serv_addr;
	if((m_sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		ret_status = ConnectionStatus::SocketCreationFailed;
		goto end;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(m_port);

	if(inet_pton(AF_INET, m_ip_addr, &serv_addr.sin_addr) <= 0)
	{
		ret_status = ConnectionStatus::BadServerAddress;
		goto end;
	}

	// Set non-blocking
	if ((arg = fcntl(m_sockfd, F_GETFL, NULL)) < 0)
	{
		fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno));
		exit(0);
	}
	arg |= O_NONBLOCK;
	if (fcntl(m_sockfd, F_SETFL, arg) < 0)
	{
		fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno));
		exit(0);
	}
	// Trying to connect with timeout
	if ((res=::connect(m_sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0)
	{
		if (errno == EINPROGRESS) {
			fprintf(stderr, "EINPROGRESS in connect() - selecting\n");
			do
			{
				timeout.tv_sec = 5;
				timeout.tv_usec = 0;
				FD_ZERO(&myset);
				FD_SET(m_sockfd, &myset);
				res = select(m_sockfd + 1, NULL, &myset, NULL, &timeout);
				if (res < 0 && errno != EINTR)
				{
					fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
					ret_status = ConnectionStatus::ClientConnectionToServerFailed;
					goto end;
				}
				else if (res > 0)
				{
					socklen_t lon;
					// Socket selected for write
					lon = sizeof(int);
					if (getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, (void*) (&valopt), &lon) < 0)
					{
						fprintf(stderr, "Error in getsockopt() %d - %s\n", errno, strerror(errno));
						ret_status = ConnectionStatus::ClientConnectionToServerFailed;
						goto end;
					}
					// Check the value returned...
					if (valopt)
					{
						fprintf(stderr, "Error in delayed connection() %d - %s\n", valopt, strerror(valopt));
						ret_status = ConnectionStatus::ClientConnectionToServerFailed;
						goto end;
					}
					break;
				}
				else
				{
					fprintf(stderr, "Timeout in select() - Canceling!\n");
					ret_status = ConnectionStatus::ClientConnectionToServerFailed;
					goto end;
				}
			}
			while (true);
		}
		else
		{
			fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
			ret_status = ConnectionStatus::ClientConnectionToServerFailed;
			goto end;
		}
	}
	// Set to blocking mode again...
	if ((arg = fcntl(m_sockfd, F_GETFL, NULL)) < 0) {
		fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno));
		exit(0);
	}
	arg &= (~O_NONBLOCK);
	if (fcntl(m_sockfd, F_SETFL, arg) < 0)
	{
		fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno));
		exit(0);
	}

	command_to_notify_cache_size.Command = ConnectionCommands::COMMAND_NOTIFY_CURRENT_MEMORY_SIZE;
	((uint32_t*) command_to_notify_cache_size.memory)[0] = m_cache_size;
	send(&command_to_notify_cache_size, sizeof(command_to_notify_cache_size));
	m_ping_send_thread = new std::thread(&TcpClient::ping_thread_fn, this);
	while(!m_is_connection_alive)
		usleep(100);
	if(m_status_callback != NULL)
		m_status_callback->OnConnected();

end:
	if(m_sockfd >= 0 && ret_status != ConnectionStatus::Success)
	{
		::close(m_sockfd);
		m_sockfd = -1;
	}

	return ret_status;
}

bool TcpClient::send(void *data, uint32_t size)
{
	int read_size;
	static std::mutex send_mutex;
	// each send need have him receive, so it wrong create this function asyncronized!
	std::lock_guard<std::mutex> guard(send_mutex);

	if(size > m_connected_cache_size)
	{
		ConnectionCommand resize_cache_command;

		printf(
			"%u(client): will send data size bigger of server_cache! (%zu/%zu) send COMMAND_SET_MEMORY_SIZE_CAPACITY!\n",
			m_sockfd, m_connected_cache_size, size
		);
		resize_cache_command.Command = ConnectionCommands::COMMAND_SET_MEMORY_SIZE_CAPACITY;
		((uint32_t*)resize_cache_command.memory)[0] = size + 1024;
		send_wrapper(m_sockfd, &resize_cache_command, sizeof(resize_cache_command));
		read_size = receive_wrapper(m_sockfd, m_cache, m_cache_size);

		// TODO CRITICAL!- this line repeat already existed lines below!!!!!!!!!!
		if(((ConnectionCommand*)m_cache)->Command == COMMAND_SET_MEMORY_SIZE_CAPACITY)
		{
			uint32_t new_size = ((uint32_t *)(((ConnectionCommand*)m_cache)->memory))[0];
			static const uint32_t MAX_ALLOCATION_SIZE = 100*1024*1024; // 100 MB

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
				printf("%u: update cache memory from %zu to %zu\n", m_sockfd, m_cache_size, new_size);
				free(m_cache);
				if((m_cache=malloc(new_size)) == NULL)
				{
					fprintf(stderr, "Have serious problem - FAIL REALLOC MEMORY!\n");
					exit(137);
				}
				m_cache_size = new_size;
			}
			// after system command we still wait to actual data
			read_size = receive_wrapper(m_sockfd, m_cache, m_cache_size);
		}

		if(read_size == sizeof(ConnectionCommand) && strcmp(((ConnectionCommand*)m_cache)->MAGIC_KEY, "SmartRtpConnectionCommand") == 0)
		{
			// TODO - this line repeat already existed lines below
			if(((ConnectionCommand*)m_cache)->Command == COMMAND_NOTIFY_CURRENT_MEMORY_SIZE)
				m_connected_cache_size = ((uint32_t *)(((ConnectionCommand*)m_cache)->memory))[0];
			else
			{
				fprintf(
					stderr, "%u: unexpected error in COMMAND_SET_MEMORY_SIZE_CAPACITY case (command=%d)!\n", m_sockfd, ((ConnectionCommand*)m_cache)->Command
				);
				exit(124);
			}
		}
		else
		{
			fprintf(stderr, "unexpected error in COMMAND_SET_MEMORY_SIZE_CAPACITY case_2!\n");
			fprintf(stderr, "read_size=%d (%d) first_bytes=%.30s!\n", read_size, sizeof(read_size), (const char *)m_cache);
			exit(125);
		}
	}

	send_wrapper(m_sockfd, data, size);
	read_size = receive_wrapper(m_sockfd, m_cache, m_cache_size);
	m_last_received_time = getUseconds();
	if(read_size == sizeof(ConnectionCommand) && strcmp(((ConnectionCommand*)m_cache)->MAGIC_KEY, "SmartRtpConnectionCommand") == 0)
	{
		if(((ConnectionCommand*)m_cache)->Command == COMMAND_SET_MEMORY_SIZE_CAPACITY)
		{
			uint32_t new_size = ((uint32_t *)(((ConnectionCommand*)m_cache)->memory))[0];
			static const uint32_t MAX_ALLOCATION_SIZE = 100*1024*1024; // 100 MB

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
				printf("%u: update cache memory from %zu to %zu\n", m_sockfd, m_cache_size, new_size);
				free(m_cache);
				if((m_cache=malloc(new_size)) == NULL)
				{
					fprintf(stderr, "Have serious problem - FAIL REALLOC MEMORY!\n");
					exit(137);
				}
				m_cache_size = new_size;
			}
			// after system command we still wait to actual data
			read_size = receive_wrapper(m_sockfd, m_cache, m_cache_size);
		}
		else if(((ConnectionCommand*)m_cache)->Command == COMMAND_NOTIFY_CURRENT_MEMORY_SIZE)
			m_connected_cache_size = ((uint32_t *)(((ConnectionCommand*)m_cache)->memory))[0];
		else if(((ConnectionCommand*)m_cache)->Command == COMMAND_ALIVE)
		{
			// Do nothing in this case - this is repeating ping thread
			// TODO add disconnection thread
			// that check last receive and kill the client someself if connection failed
			return true;
		}
		else if(((ConnectionCommand*)m_cache)->Command == COMMAND_NOT_FOUND)
		{
			printf("%u: ERROR - COMMAND_NOT_FOUND!\n", m_sockfd);
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
uint32_t TcpClient::receive_data_size(){return m_last_received_size;}
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
		delete m_ping_send_thread;
		m_ping_send_thread = NULL;
	}
	if(m_sockfd >= 0)
	{
		::close(m_sockfd);
		m_sockfd = -1;
	}
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
	printf("%u: ping_thread_fn finish!\n", m_sockfd);
}

const char *TcpClient::ip(){return m_ip_addr;}
const uint16_t TcpClient::port(){return m_port;}
