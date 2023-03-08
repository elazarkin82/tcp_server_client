#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <thread>
#include <mutex>

#include <smart_rtp/tcp/TcpServer.h>
#include <smart_rtp/tcp/TcpClient.h>
#include "testcase_interface.hpp"

class TestClientConnection: public BaseTestInterface
{
private:
	class OnClientReceiveCallback: public TcpClient::OnConnectionStatusCallback
	{
	private:
		TcpClient *m_parent;
	public:
		OnClientReceiveCallback(TcpClient *parent){m_parent = parent;}
		virtual ~OnClientReceiveCallback() {};
		void OnConnected() {printf("Server connected! %s:%d\n", m_parent->ip(), m_parent->port());};
		void OnDisconnected() {printf("Server disconnected!\n");};
		void OnDataReceived(void *data, size_t) {};
	};

	class OnServerReceiveCallback: public TcpServer::OnReceiveCallback
	{
	private:
		TcpServer *m_parent;
	public:
		OnServerReceiveCallback(TcpServer *parent) {;m_parent = parent;}
		virtual ~OnServerReceiveCallback() {}

		void onConnected(uint64_t client_id)
		{
			printf("client %zu connected!\n", client_id);
		}
		void onDisconnected(uint64_t client_id)
		{
			printf("client %zu disconnected!\n", client_id);
		}
		void onRequestToRepeat(TcpServer::Client *client, void *data, size_t size)
		{
			const char test1_header[] = "hello from client!";
			const char test2_header[] = "test_2_";
			if(strncmp((char *)data, test1_header, sizeof(test1_header)) == 0)
			{
				char out[] = "hello from server!";
				printf("server_onRequestToRepeat: %s(%zu)\n", (char *)data, size);
				m_parent->send(client, out, sizeof(out));
			}
			else if(strncmp((char *)data, test2_header, sizeof(test2_header) - 1) == 0)
			{
				char *temp = (char *) malloc(size);
				for(int i = 0; i < size; i++)
					temp[i] = ((char *)data)[size - i -1];
				m_parent->send(client, temp, size);
				free(temp);
			}
			else
			{
				printf("server_onRequestToRepeat(unknow request): %.10s...(%zu)\n", (char *)data, size);
				m_parent->send(client, data, size);
			}
		}
	};

	TcpServer *m_server;
	bool m_server_up;
	bool m_test_success;
	std::thread *m_server_thread;
public:
	TestClientConnection()
	{
		m_server_up = false;
		m_test_success = true;
		m_server_thread = NULL;
		m_server = NULL;
	}

	virtual ~TestClientConnection()
	{
		if(m_server)
			delete m_server;

		if(m_server_thread)
			delete m_server_thread;
	}
private:

	void server_thread()
	{
		ConnectionStatus status;
		OnServerReceiveCallback callback(m_server);
		if(m_server == NULL)
			m_server = new TcpServer(9125);

		m_server_up = true;
		printf("will start server!\n");
		if((status=m_server->start(&callback)) != ConnectionStatus::FinishSuccess)
		{
			fprintf(stderr, "Fail start server!\n");
			m_test_success = false;
			m_server_up = false;
		}
		printf("test_client_connection: server_thread finish!\n");
	}
public:
	bool test()
	{
		TcpClient client("127.0.0.1", 9125);
		OnClientReceiveCallback callback(&client);
		ConnectionStatus status;
		char data[65536];
		memset(data, 0, sizeof(data));

		printf("test client connection started!\n");

//		printf("1. test client before server up!\n");
//		if((status=client.connect(&callback)) != ConnectionStatus::ClientConnectionToServerFailed)
//		{
//			m_test_success = false;
//			goto end;
//		}
//		printf("\tsuccess fail!\n");

		m_server_thread = new std::thread(&TestClientConnection::server_thread, this);
		while(!m_server_up && m_test_success)
			usleep(100);

		// wait little to get server up!
		sleep(2);

		if((status=client.connect(&callback)) != ConnectionStatus::Success)
		{
			TcpServer::print_status_name(status);
			m_test_success = false;
			goto end;
		}

		// Test1 Hello!
		memset(data, 0, sizeof(data));
		sprintf(data, "hello from client!");
		client.send(data, strlen(data));
		printf("server answer: %s\n", (const char *)client.received_data());
		if(strcmp((const char *)client.received_data(), "hello from server!") != 0)
		{
			m_test_success = false;
			goto end;
		}

		// Test2 - resize cache! Send random data and expect receive reverse data and cache_size of server and client equal to sizeof(data)
		for(int i = 0; i < sizeof(data); i++)
		{
			const char test2_header[] = "test_2_";
			if(i < strlen(test2_header))
				data[i] = test2_header[i];
			else
				data[i] = (char)(32 + rand()%80);
		}
		client.send(data, sizeof(data));
		for(int i = 0; i < sizeof(data); i++)
		{
			int index = (int)(sizeof(data) - i - 1);
			if(((char*)client.received_data())[i] != data[index])
			{
				m_test_success = false;
				goto end;
			}
		}

end:
		client.disconnect();
		if(m_server && m_server_up)
			m_server->close();
		if(m_server_thread->joinable())
			m_server_thread->join();
		return m_test_success;
	}
};
