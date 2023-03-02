//
// Created by elazarkin on 24/02/2023.
//

#ifndef MAFHIDYONIM_DEFENITIONS_H
#define MAFHIDYONIM_DEFENITIONS_H

enum ConnectionStatus{
	Success,
    FinishSuccess,
    SocketCreationFailed,
    SocketBindPortFailed,
    SocketListenFailed,
    ServerSocketConnectionTimeout,
    ClientConnectionToServerFailed,
    ConnectionInitBadProtocol
};

#endif //MAFHIDYONIM_DEFENITIONS_H
