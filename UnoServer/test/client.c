#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <windows.h>

#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")


#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080


/*
 * ========================================================
 * FUNCOES DE COMUNICACAO
 * ========================================================
 */

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


/*
 * ========================================================
 * IMPRESSAO DO ESTADO
 * ========================================================
 */

void imprimirEstado(
    const EstadoJogo *estado
) {
    printf("\n");
    printf("==================================================\n");
    printf("                  ESTADO DO JOGO\n");
    printf("==================================================\n");

    printf("\n");
    printf("JOGADOR\n");
    printf("------------------------------------------\n");

    printf(
        "ID: %d\n",
        estado->jogador.id
    );

    printf(
        "Nome: %s\n",
        estado->jogador.nome
    );

    printf(
        "Quantidade de cartas: %d\n",
        estado->jogador.qtdCartas
    );


    printf("\n");
    printf("SUAS CARTAS\n");
    printf("------------------------------------------\n");

    for (
        int i = 0;
        i < estado->jogador.qtdCartas;
        i++
    ) {
        printf(
            "%d - ID: %d | cor: %d | simbolo: %s\n",
            i + 1,
            estado->jogador.cartas[i].id,
            estado->jogador.cartas[i].cor,
            estado->jogador.cartas[i].simbolo
        );
    }


    printf("\n");
    printf("CARTAS POSSIVEIS DE JOGAR\n");
    printf("------------------------------------------\n");

    if (
        estado->jogador.qtdCartasPossiveis == 0
    ) {
        printf("Nenhuma carta possivel.\n");
    }
    else {

        for (
            int i = 0;
            i < estado->jogador.qtdCartasPossiveis;
            i++
        ) {
            printf(
                "%d - Carta ID %d\n",
                i + 1,
                estado->jogador.idsCartasPossiveis[i]
            );
        }
    }


    printf("\n");
    printf("PARTIDA\n");
    printf("------------------------------------------\n");

    printf(
        "Rodada: %d\n",
        estado->partida.numeroRodada
    );

    printf(
        "Sua vez: %s\n",
        estado->partida.suaVez
            ? "SIM"
            : "NAO"
    );

    printf(
        "Tempo para jogar: %d ms\n",
        estado->partida.tempoMs
    );


    printf("\n");
    printf("ULTIMA CARTA\n");
    printf("------------------------------------------\n");

    printf(
        "ID: %d | cor: %d | simbolo: %s\n",
        estado->partida.ultimaCarta.id,
        estado->partida.ultimaCarta.cor,
        estado->partida.ultimaCarta.simbolo
    );


    printf("\n");
    printf("ADVERSARIO\n");
    printf("------------------------------------------\n");

    printf(
        "Nome: %s\n",
        estado->partida.nomeAdversario
    );

    printf(
        "Quantidade de cartas: %d\n",
        estado->partida.numeroCartasAdversario
    );

    printf("\n");
    printf("==================================================\n");
}


/*
 * ========================================================
 * RECEBER MENSAGEM
 * ========================================================
 */

int receberMensagem(
    SOCKET socket,
    Mensagem *response
) {
    int resultado = recvAll(
        socket,
        (char *) response,
        sizeof(Mensagem)
    );

    if (resultado == 0) {

        printf(
            "\nServidor desconectou.\n"
        );

        return 0;
    }

    if (resultado < 0) {

        printf(
            "\nErro ao receber mensagem. "
            "WSAError: %d\n",
            WSAGetLastError()
        );

        return -1;
    }

    return 1;
}


/*
 * ========================================================
 * ENVIAR ACAO
 * ========================================================
 */

int enviarAcao(
    SOCKET socket,
    int jogadorId,
    TipoAcao acao,
    int cartaId
) {
    Solicitacao request;

    memset(
        &request,
        0,
        sizeof(request)
    );

    request.jogadorId = jogadorId;
    request.acao = acao;
    request.cartaId = cartaId;


    printf("\n");
    printf("Enviando acao...\n");

    printf(
        "  jogadorId: %d\n",
        request.jogadorId
    );

    printf(
        "  acao: %d\n",
        request.acao
    );

    printf(
        "  cartaId: %d\n",
        request.cartaId
    );


    if (
        enviarTudo(
            socket,
            (const char *) &request,
            sizeof(request)
        ) < 0
    ) {

        printf(
            "Erro ao enviar acao. "
            "WSAError: %d\n",
            WSAGetLastError()
        );

        return -1;
    }

    return 0;
}


/*
 * ========================================================
 * MAIN
 * ========================================================
 */

int main(void)
{
    WSADATA wsaData;

    SOCKET socketCliente;

    struct sockaddr_in serverAddress;


    /*
     * ====================================================
     * WINSOCK
     * ====================================================
     */

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        ) != 0
    ) {

        printf(
            "Erro ao inicializar Winsock.\n"
        );

        return EXIT_FAILURE;
    }


    /*
     * ====================================================
     * SOCKET
     * ====================================================
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
     * ====================================================
     * ENDERECO DO SERVIDOR
     * ====================================================
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
     * ====================================================
     * CONNECT
     * ====================================================
 */

    printf(
        "Conectando ao servidor...\n"
    );

    if (
        connect(
            socketCliente,
            (struct sockaddr *) &serverAddress,
            sizeof(serverAddress)
        ) == SOCKET_ERROR
    ) {

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
     * ====================================================
     * AGUARDAR INICIO DA PARTIDA
     * ====================================================
     */

    printf(
        "\nAguardando inicio da partida...\n"
    );


    Mensagem response;

    int resultado =
        receberMensagem(
            socketCliente,
            &response
        );


    if (resultado <= 0) {

        closesocket(socketCliente);
        WSACleanup();

        return EXIT_FAILURE;
    }


    /*
     * A primeira mensagem obrigatoriamente
     * deve ser MSG_PARTIDA_INICIADA.
     */

    if (
        response.tipo !=
        MSG_PARTIDA_INICIADA
    ) {

        printf(
            "\nERRO DE PROTOCOLO!\n"
        );

        printf(
            "Esperado: MSG_PARTIDA_INICIADA\n"
        );

        printf(
            "Recebido: %d\n",
            response.tipo
        );

        closesocket(socketCliente);
        WSACleanup();

        return EXIT_FAILURE;
    }


    /*
     * ====================================================
     * PARTIDA INICIADA
     * ====================================================
     */

    printf("\n");
    printf("**************************************************\n");
    printf("*              PARTIDA INICIADA                 *\n");
    printf("**************************************************\n");


    imprimirEstado(
        &response.estado
    );


    /*
     * ====================================================
     * LOOP DA PARTIDA
     * ====================================================
     */

    while (1) {

        /*
         * -----------------------------------------------
         * Verifica se e a vez deste jogador.
         * -----------------------------------------------
         */

        if (
            response.estado.partida.suaVez
        ) {

            printf("\n");
            printf("Sua vez!\n");
            printf("\n");

            printf(
                "1 - Jogar carta\n"
            );

            printf(
                "2 - Comprar carta\n"
            );

            printf(
                "Escolha: "
            );


            int opcao;

            scanf(
                "%d",
                &opcao
            );


            /*
             * -------------------------------------------
             * JOGAR CARTA
             * -------------------------------------------
             */

            if (opcao == 1) {

                int cartaId;

                printf(
                    "Digite o ID da carta: "
                );

                scanf(
                    "%d",
                    &cartaId
                );


                if (
                    enviarAcao(
                        socketCliente,
                        response.estado.jogador.id,
                        ACAO_JOGAR_CARTA,
                        cartaId
                    ) != 0
                ) {
                    break;
                }
            }


            /*
             * -------------------------------------------
             * COMPRAR CARTA
             * -------------------------------------------
             */

            else if (opcao == 2) {

                if (
                    enviarAcao(
                        socketCliente,
                        response.estado.jogador.id,
                        ACAO_COMPRAR_CARTA,
                        0
                    ) != 0
                ) {
                    break;
                }
            }


            /*
             * -------------------------------------------
             * OPCAO INVALIDA
             * -------------------------------------------
             */

            else {

                printf(
                    "Opcao invalida.\n"
                );

                continue;
            }


            /*
             * -------------------------------------------
             * AGUARDAR RESPOSTA
             * -------------------------------------------
             */

            resultado =
                receberMensagem(
                    socketCliente,
                    &response
                );

            if (resultado <= 0) {
                break;
            }


            /*
             * -------------------------------------------
             * PROCESSAR RESPOSTA
             * -------------------------------------------
             */

            if (
                response.tipo ==
                MSG_JOGADA_INVALIDA
            ) {

                printf("\n");
                printf(
                    ">>> JOGADA INVALIDA <<<\n"
                );

                imprimirEstado(
                    &response.estado
                );

                continue;
            }


            if (
                response.tipo ==
                MSG_ESTADO_JOGO
            ) {

                imprimirEstado(
                    &response.estado
                );

                continue;
            }


            if (
                response.tipo ==
                MSG_PARTIDA_FINALIZADA
            ) {

                printf("\n");
                printf(
                    ">>> PARTIDA FINALIZADA <<<\n"
                );

                imprimirEstado(
                    &response.estado
                );

                break;
            }


            if (
                response.tipo ==
                MSG_JOGADOR_SAIU
            ) {

                printf("\n");
                printf(
                    ">>> JOGADOR SAIU DA PARTIDA <<<\n"
                );

                imprimirEstado(
                    &response.estado
                );

                break;
            }


            printf(
                "\nMensagem desconhecida: %d\n",
                response.tipo
            );
        }

        /*
         * -----------------------------------------------
         * NAO E A VEZ DESTE JOGADOR
         * -----------------------------------------------
         */

        else {

            printf("\n");
            printf(
                "Aguardando jogada do adversario...\n"
            );


            resultado =
                receberMensagem(
                    socketCliente,
                    &response
                );

            if (resultado <= 0) {
                break;
            }


            if (
                response.tipo ==
                MSG_ESTADO_JOGO
            ) {

                imprimirEstado(
                    &response.estado
                );

                continue;
            }


            if (
                response.tipo ==
                MSG_JOGADA_INVALIDA
            ) {

                printf(
                    "\n>>> Jogada invalida recebida. <<<\n"
                );

                imprimirEstado(
                    &response.estado
                );

                continue;
            }


            if (
                response.tipo ==
                MSG_PARTIDA_FINALIZADA
            ) {

                printf("\n");
                printf(
                    ">>> PARTIDA FINALIZADA <<<\n"
                );

                imprimirEstado(
                    &response.estado
                );

                break;
            }


            if (
                response.tipo ==
                MSG_JOGADOR_SAIU
            ) {

                printf("\n");
                printf(
                    ">>> JOGADOR SAIU DA PARTIDA <<<\n"
                );

                break;
            }


            printf(
                "\nMensagem desconhecida: %d\n",
                response.tipo
            );
        }
    }


    /*
     * ====================================================
     * ENCERRAMENTO
     * ====================================================
 */

    printf(
        "\nDesconectando do servidor...\n"
    );

    closesocket(
        socketCliente
    );

    WSACleanup();

    return EXIT_SUCCESS;
}
