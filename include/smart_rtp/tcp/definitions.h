//
// Created by elazarkin on 24/02/2023.
//

#ifndef MAFHIDYONIM_DEFENITIONS_H
#define MAFHIDYONIM_DEFENITIONS_H

enum ConnectionStatus
{
	Success,
    FinishSuccess,
    SocketCreationFailed,
    SocketBindPortFailed,
	BadServerAddress,
    SocketListenFailed,
    ServerSocketConnectionTimeout,
    ClientConnectionToServerFailed,
    ConnectionInitBadProtocol
};

void print_status_name(ConnectionStatus status)
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

#endif //MAFHIDYONIM_DEFENITIONS_H
