#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <winsock2.h>
#include "protocol.h"

int enviarTudo(SOCKET socket, const char *buffer, int tamanho);

int recvAll(SOCKET socket, char *buffer, int tamanho);

bool enviarSolicitacao(
    SOCKET socket,
    int jogadorId,
    TipoAcao acao,
    int cartaId
);

#endif
