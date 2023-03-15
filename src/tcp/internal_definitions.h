/*
 * internal_definitions.h
 *
 *  Created on: 2 Mar 2023
 *      Author: elazarkin
 */

#ifndef SRC_TCP_INTERNAL_DEFINITIONS_H_
#define SRC_TCP_INTERNAL_DEFINITIONS_H_

#include <sys/socket.h>

enum ConnectionCommands
{
	COMMAND_SET_MEMORY_SIZE_CAPACITY = 1,
	COMMAND_NOTIFY_CURRENT_MEMORY_SIZE,
	COMMAND_ALIVE,
	COMMAND_SUCCESS,
	COMMAND_NOT_FOUND,
	COMMAND_ERROR,
	COMMAND_CRITICAL_ERROR_EXIT,
};

struct ConnectionCommand
{
    ConnectionCommand(): MAGIC_KEY("SmartRtpConnectionCommand"), Command(COMMAND_SUCCESS) {}
    const char MAGIC_KEY[32];
    ConnectionCommands Command;
    char memory[128];
};

inline void send_wrapper(int sockfd, const void *data, uint32_t size)
{
#ifdef DEBUG_PRINTS
	printf("%d: send_wrapper send_prepare %zu(%zu)\n", sockfd, size, sizeof(size));
#endif
	::send(sockfd, &size, sizeof(size), 0);
#ifdef DEBUG_PRINTS
	printf("%d: send_wrapper send %.10s... (%zu)\n", sockfd, (char *) data, size);
#endif
	::send(sockfd, data, size, 0);
}

inline uint32_t receive_wrapper(int sockfd, void *cache, uint32_t max_size)
{
	uint32_t ret;
	uint32_t read_size = sizeof(ret);

	while(read_size > 0)
	{
		int curr_read_size = ::read(sockfd, &ret, sizeof(ret));
		if(curr_read_size > 0)
			read_size -= curr_read_size;
		else
			return curr_read_size;
	}

#ifdef DEBUG_PRINTS
	printf("%d: send_wrapper receive_prepare %zu(%zu)\n", sockfd, ret, sizeof(ret));
#endif

	read_size = ret;
	memset(cache, 0, max_size);
	while(read_size > 0)
	{
		int curr_read_size = ::read(sockfd, cache, read_size);
		if(curr_read_size > 0)
			read_size -= curr_read_size;
		else
			return curr_read_size;
	}
#ifdef DEBUG_PRINTS
	printf("%d: send_wrapper receive %.10s... (%zu, read_size at and = %zu <- should be 0)\n", sockfd, (char *) cache, ret, (read_size));
#endif
	return ret;
}

#endif /* SRC_TCP_INTERNAL_DEFINITIONS_H_ */
