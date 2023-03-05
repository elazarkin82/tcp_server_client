#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <execinfo.h>

#include <thread>
#include <mutex>

#include <smart_rtp/tcp/TcpServer.h>
#include <smart_rtp/tcp/TcpClient.h>

#include "test_cases/test_service_up_down.h"

struct termios tty_attr;
tcflag_t backup_c_lflag;

void sig_handler(int sig)
{
	void *array[10];
	size_t size = backtrace(array, 10);
	tty_attr.c_cflag = backup_c_lflag;
	if (tcsetattr(STDIN_FILENO, 0, &tty_attr) < 0)
		fprintf(stderr, "tcgetattr failed!\n");

	printf("sig_handler %d\n", sig);
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(array, size, STDERR_FILENO);
	for(int i = 0; i < size; i++)
		printf("0x%04x\n", (uintptr_t)array[i]);
	exit(sig);
}

// TODO
//  0. run simple connection with key interrupt for close option
//  1. test connection disconnection few times, server down/up/down/up, etc
//  2. test few connections
//  3. test ping thread disconnection
//  4. test request repeat
//  5. test request without repeat (internal repeating in this case)
//  6. test internal messages (notify cache size and increase cache size)

std::mutex temporary_main_locker;
bool key_listener_run_flag = false;

void key_listener()
{
	struct termios t;
	temporary_main_locker.lock();

	if (tcgetattr(STDIN_FILENO, &tty_attr) < 0)
	{
		fprintf(stderr, "tcgetattr failed!\n");
		goto end;
	}

	backup_c_lflag = tty_attr.c_lflag;
	tty_attr.c_lflag &= ~ICANON;
	tty_attr.c_lflag &= ~ECHO;

	if (tcsetattr(STDIN_FILENO, 0, &tty_attr) < 0)
	{
		fprintf(stderr, "tcsetattr failed!\n");
		goto end;
	}

	key_listener_run_flag = true;
	while(key_listener_run_flag)
	{
		int key = getchar();
		if(key == 27)
		{
			key_listener_run_flag = false;
		}
		else printf("key=%d\n", key);
	}
	tty_attr.c_lflag = backup_c_lflag;
	if(tcsetattr(STDIN_FILENO, 0, &tty_attr) < 0)
	{
		fprintf(stderr, "tcsetattr at the end failed!\n");
		goto end;
	}
end:
	temporary_main_locker.unlock();
}

bool test_client_connection()
{
	return true;
}

int main(int argc, char **argv)
{
	std::thread key_listener_thread(key_listener);
	signal(SIGINT, sig_handler);
	signal(SIGSEGV, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGABRT, sig_handler);
	signal(SIGTRAP, sig_handler);
	while(!key_listener_run_flag)
		usleep(1000);

	 printf("test_server_up_down: %s\n", TestServerUpDown(3).test() ? "Success": "Fail");
//	printf("test_client_connection: %s\n", test_client_connection() ? "Success": "Fail");
	temporary_main_locker.lock();
	if(key_listener_thread.joinable())
		key_listener_thread.join();
	printf("TODO - implement tester\n");
	return 0;
}
