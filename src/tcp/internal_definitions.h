/*
 * internal_definitions.h
 *
 *  Created on: 2 Mar 2023
 *      Author: elazarkin
 */

#ifndef SRC_TCP_INTERNAL_DEFINITIONS_H_
#define SRC_TCP_INTERNAL_DEFINITIONS_H_

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


#endif /* SRC_TCP_INTERNAL_DEFINITIONS_H_ */
