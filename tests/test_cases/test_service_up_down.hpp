/*
 * test_service_up_down.h
 *
 *  Created on: 3 Mar 2023
 *      Author: elazarkin
 */

#ifndef TESTS_TEST_CASES_TEST_SERVICE_UP_DOWN_HPP_
#define TESTS_TEST_CASES_TEST_SERVICE_UP_DOWN_HPP_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <thread>
#include <mutex>

#include <smart_rtp/tcp/TcpServer.h>
#include <smart_rtp/tcp/TcpClient.h>
#include "testcase_interface.hpp"

class TestServerUpDown: public BaseTestInterface
{
private:
	int m_times;
public:
	TestServerUpDown(int times)
	{
		m_times = times;
	}

	virtual ~TestServerUpDown(){}

private:
	void close_server_thread(TcpServer *server, uint32_t secs)
	{
		for(int i = secs; i > 0; i--)
		{
			printf("Will kill server after %d secs\n", i);
			sleep(1);
		}
		server->close();
	}

public:
	bool test()
	{
		TcpServer server(9125);
		for(int i = 0; i < m_times; i++)
		{
			std::thread close_thread(&TestServerUpDown::close_server_thread, this, &server, 2);
			ConnectionStatus status;
			if((status=server.start(NULL)) != ConnectionStatus::FinishSuccess)
			{
				TcpServer::print_status_name(status);
				if(close_thread.joinable())
					close_thread.join();
				return false;
			}
			if(close_thread.joinable())
				close_thread.join();
		}
		return true;
	}
};


#endif /* TESTS_TEST_CASES_TEST_SERVICE_UP_DOWN_HPP_ */
