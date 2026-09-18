#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <windows.h>

#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")


#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080


int enviarTudo(
    SOCKET socket,
    const char *buffer,
    int tamanho
) {
    int totalEnviado = 0;

    while (totalEnviado < tamanho) {
        int enviado = send(
            socket,
            buffer + totalEnviado,
            tamanho - totalEnviado,
            0
        );

        if (enviado == SOCKET_ERROR) {
            return -1;
        }

        totalEnviado += enviado;
    }

    return totalEnviado;
}

int recvAll(
    SOCKET socket,
    char *buffer,
    int tamanho
) {
    int totalRecebido = 0;

    while (totalRecebido < tamanho) {
        int recebido = recv(
            socket,
            buffer + totalRecebido,
            tamanho - totalRecebido,
            0
        );

        if (recebido == 0) {
            return 0;
        }

        if (recebido == SOCKET_ERROR) {
            return -1;
        }

        totalRecebido += recebido;
    }

    return totalRecebido;
}

int main(void) {
    WSADATA wsaData;

    SOCKET socketCliente;

    struct sockaddr_in serverAddress;


    /*
     * ========================================================
     * WINSOCK
     * ========================================================
     */

    if (WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        ) != 0) {
        printf(
            "Erro ao inicializar Winsock.\n"
        );

        return EXIT_FAILURE;
    }


    /*
     * ========================================================
     * SOCKET
     * ========================================================
     */

    socketCliente = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
    );

    if (
        socketCliente ==
        INVALID_SOCKET
    ) {
        printf(
            "Erro ao criar socket.\n"
        );

        WSACleanup();

        return EXIT_FAILURE;
    }


    /*
     * ========================================================
     * ENDERECO DO SERVIDOR
     * ========================================================
     */

    memset(
        &serverAddress,
        0,
        sizeof(serverAddress)
    );

    serverAddress.sin_family =
            AF_INET;

    serverAddress.sin_port =
            htons(SERVER_PORT);

    serverAddress.sin_addr.s_addr =
            inet_addr(SERVER_IP);


    /*
     * ========================================================
     * CONNECT
     * ========================================================
     */

    printf(
        "Conectando ao servidor...\n"
    );

    if (connect(
            socketCliente,
            (struct sockaddr *) &serverAddress,
            sizeof(serverAddress)
        ) == SOCKET_ERROR) {
        printf(
            "Erro ao conectar. "
            "WSAError: %d\n",
            WSAGetLastError()
        );

        closesocket(socketCliente);

        WSACleanup();

        return EXIT_FAILURE;
    }


    printf(
        "Conectado ao servidor!\n"
    );


    /*
     * ========================================================
     * SOLICITACAO
     * ========================================================
     */

    Solicitacao request;

    memset(
        &request,
        0,
        sizeof(request)
    );


    request.jogadorId = 0;

    request.acao =
            ACAO_JOGAR_CARTA;

    request.cartaId = 5;


    printf(
        "Enviando solicitacao...\n"
    );

    printf(
        "  jogadorId = %d\n",
        request.jogadorId
    );

    printf(
        "  acao      = %d\n",
        request.acao
    );

    printf(
        "  cartaId   = %d\n",
        request.cartaId
    );


    /*
     * ========================================================
     * SEND
     * ========================================================
     */

    if (enviarTudo(
            socketCliente,
            (const char *) &request,
            sizeof(request)
        ) < 0) {
        printf(
            "Erro ao enviar solicitacao. "
            "WSAError: %d\n",
            WSAGetLastError()
        );

        closesocket(socketCliente);

        WSACleanup();

        return EXIT_FAILURE;
    }


    printf(
        "Solicitacao enviada!\n"
    );


    /*
 * ========================================================
 * RECEBER RESPOSTA
 * ========================================================
 */

    Mensagem response;

    int resultado = recvAll(
        socketCliente,
        (char *) &response,
        sizeof(Mensagem)
    );


    if (resultado == 0) {
        printf(
            "Servidor desconectou.\n"
        );

        closesocket(socketCliente);
        WSACleanup();

        return EXIT_FAILURE;
    }


    if (resultado < 0) {
        printf(
            "Erro ao receber resposta. "
            "WSAError: %d\n",
            WSAGetLastError()
        );

        closesocket(socketCliente);
        WSACleanup();

        return EXIT_FAILURE;
    }


    printf(
        "Resposta recebida!\n"
    );

    printf(
        "  tipo: %d\n",
        response.tipo
    );

    printf(
        "  jogador: %d\n",
        response.estado.jogador.id + 1
    );

    printf(
        "  rodada: %d\n",
        response.estado.partida.numeroRodada
    );

    printf(
        "  sua vez: %s\n",
        response.estado.partida.suaVez
            ? "SIM"
            : "NAO"
    );

    printf(
        "  cartas do adversario: %d\n",
        response.estado.partida.numeroCartasAdversario
    );

    printf(
        "  adversario: %s\n",
        response.estado.partida.nomeAdversario
    );


    /*
     * ========================================================
     * AGUARDAR
     * ========================================================
     */

    printf(
        "Pressione ENTER para desconectar...\n"
    );

    getchar();


    /*
     * ========================================================
     * ENCERRAMENTO
     * ========================================================
     */

    closesocket(
        socketCliente
    );

    WSACleanup();


    return EXIT_SUCCESS;
}
