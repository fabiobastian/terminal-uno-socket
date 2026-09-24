/**
 * @file:       main.c
 * @author:     Nathan Berger
 * @date:       2026-09-20
 * @version     2.1
 * @brief       Client responsible for rendering, network communication and
 *              input handling for the terminal UNO game. Windows only.
 *
 * MIT LICENSE
 *
 * Copyright (c) 2026 Fábio Júnior Nielsson Bastian.
 * Unauthorized copying or use of this file is prohibited.
 */

#include <winsock2.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include "../include/protocol.h"
#include "../include/ui.h"
#include "../include/network.h"

#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 8080
#define STATUS_MSG_DURATION_MS 2500
#define STDOUT_BUFFER_SIZE 65536
#define DEBUG_LOG_FILE "client_debug.log"

typedef struct {
    CRITICAL_SECTION lock;
    EstadoJogo estado;
    bool hasEstado;
    bool connectionLost;
    bool partidaFinalizada;
    bool adversarioSaiu;

    char vencedor[MAX_NOME_JOGADOR];

    char statusMsg[128];
    DWORD statusMsgExpiry;
} SharedState;

static SharedState g_shared;
static CRITICAL_SECTION g_logLock;

void debugLog(const char *fmt, ...) {
    EnterCriticalSection(&g_logLock);

    FILE *file = fopen(DEBUG_LOG_FILE, "a");

    if (file != NULL) {
        SYSTEMTIME time;
        GetLocalTime(&time);

        fprintf(
            file,
            "[%02d:%02d:%02d.%03d] ",
            time.wHour,
            time.wMinute,
            time.wSecond,
            time.wMilliseconds
        );

        va_list args;
        va_start(args, fmt);
        vfprintf(file, fmt, args);
        va_end(args);

        fputc('\n', file);
        fclose(file);
    }

    LeaveCriticalSection(&g_logLock);
}

static const char *tipoMensagemParaTexto(TipoMensagem tipo) {
    switch (tipo) {
        case MSG_PARTIDA_INICIADA: return "MSG_PARTIDA_INICIADA";
        case MSG_ESTADO_JOGO: return "MSG_ESTADO_JOGO";
        case MSG_JOGADA_INVALIDA: return "MSG_JOGADA_INVALIDA";
        case MSG_PARTIDA_FINALIZADA: return "MSG_PARTIDA_FINALIZADA";
        case MSG_JOGADOR_SAIU: return "MSG_JOGADOR_SAIU";
        default: return "DESCONHECIDO";
    }
}

static DWORD WINAPI recvThreadProc(LPVOID param) {
    SOCKET socket = (SOCKET) (uintptr_t) param;

    while (1) {
        Mensagem message;
        int result = recvAll(
            socket,
            (char *) &message,
            sizeof(message)
        );

        if (result <= 0) {
            debugLog(
                "RECEBIDO -> recvAll retornou %d (WSAError=%d)",
                result,
                WSAGetLastError()
            );

            EnterCriticalSection(&g_shared.lock);
            g_shared.connectionLost = true;
            LeaveCriticalSection(&g_shared.lock);
            break;
        }

        debugLog(
            "RECEBIDO <- tipo=%s (%d) suaVez=%d jogadorId=%d qtdCartas=%d",
            tipoMensagemParaTexto(message.tipo),
            (int) message.tipo,
            message.estado.partida.suaVez,
            message.estado.jogador.id,
            message.estado.jogador.qtdCartas
        );

        EnterCriticalSection(&g_shared.lock);

        switch (message.tipo) {
            case MSG_PARTIDA_INICIADA:
            case MSG_ESTADO_JOGO:
                g_shared.estado = message.estado;
                g_shared.hasEstado = true;
                break;

            case MSG_JOGADA_INVALIDA:
                snprintf(
                    g_shared.statusMsg,
                    sizeof(g_shared.statusMsg),
                    "Jogada invalida!"
                );
                g_shared.statusMsgExpiry =
                        GetTickCount() + STATUS_MSG_DURATION_MS;
                break;

            case MSG_PARTIDA_FINALIZADA:
                g_shared.estado = message.estado;
                g_shared.hasEstado = true;

                if (message.estado.jogador.qtdCartas == 0) {
                    strncpy(
                        g_shared.vencedor,
                        message.estado.jogador.nome,
                        MAX_NOME_JOGADOR - 1
                    );
                } else if (message.estado.partida.numeroCartasAdversario == 0) {
                    strncpy(
                        g_shared.vencedor,
                        message.estado.partida.nomeAdversario,
                        MAX_NOME_JOGADOR - 1
                    );
                }

                g_shared.vencedor[MAX_NOME_JOGADOR - 1] = '\0';
                g_shared.partidaFinalizada = true;
                break;

            case MSG_JOGADOR_SAIU:
                g_shared.adversarioSaiu = true;
                break;

            default:
                /*
                 * Tipo de mensagem fora do enum conhecido. Antes isso era
                 * ignorado silenciosamente; agora fica registrado no log
                 * de debug para facilitar diagnosticar problemas de
                 * protocolo entre client e server.
                 */
                debugLog(
                    "RECEBIDO -> tipo de mensagem desconhecido (%d), ignorando",
                    (int) message.tipo
                );
                break;
        }

        LeaveCriticalSection(&g_shared.lock);

        if (message.tipo == MSG_PARTIDA_FINALIZADA ||
            message.tipo == MSG_JOGADOR_SAIU) {
            break;
        }
    }

    return 0;
}

static bool iniciarWinsock(void) {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Erro ao inicializar Winsock.\n");
        return false;
    }

    return true;
}

static SOCKET conectarServidor(const char *ip, int port) {
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (clientSocket == INVALID_SOCKET) {
        printf("Erro ao criar socket.\n");
        return INVALID_SOCKET;
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons((u_short) port);
    serverAddress.sin_addr.s_addr = inet_addr(ip);

    printf("Conectando a %s:%d...\n", ip, port);
    fflush(stdout);

    if (connect(
            clientSocket,
            (struct sockaddr *) &serverAddress,
            sizeof(serverAddress)
        ) == SOCKET_ERROR) {
        printf("Erro ao conectar. WSAError: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        return INVALID_SOCKET;
    }

    printf("Conectado! Aguardando estado inicial do servidor...\n");
    fflush(stdout);

    return clientSocket;
}

static bool receberMensagemInicial(SOCKET socket) {
    Mensagem message;
    int result = recvAll(socket, (char *) &message, sizeof(message));

    if (result == 0) {
        printf("\nServidor desconectou antes de iniciar a partida.\n");
        return false;
    }

    if (result < 0) {
        printf(
            "\nErro ao receber mensagem inicial. WSAError: %d\n",
            WSAGetLastError()
        );
        return false;
    }

    debugLog(
        "RECEBIDO (sincrono) <- tipo=%s (%d) suaVez=%d jogadorId=%d",
        tipoMensagemParaTexto(message.tipo),
        (int) message.tipo,
        message.estado.partida.suaVez,
        message.estado.jogador.id
    );

    if (message.tipo != MSG_PARTIDA_INICIADA) {
        printf("\nERRO DE PROTOCOLO!\n");
        printf("Esperado: MSG_PARTIDA_INICIADA\n");
        printf("Recebido: %d\n", message.tipo);
        return false;
    }

    g_shared.estado = message.estado;
    g_shared.hasEstado = true;
    return true;
}

static bool validarTela(Screen *screen) {
    if (screen->height < 40) {
        printf(
            "[ERROR]: Screen height should be at least 40 but was %d.\n",
            screen->height
        );
        return false;
    }

    if (screen->width < 150) {
        printf(
            "[ERROR]: Screen width should be at least 150 but was %d.\n",
            screen->width
        );
        return false;
    }

    return true;
}

static void processarEntrada(
    SOCKET socket,
    int jogadorId,
    const EstadoJogo *estado,
    HandUI *hand,
    Input input,
    int *running
) {
    switch (input) {
        case INPUT_RIGHT:
            if (hand->selectedCard < estado->jogador.qtdCartas - 1) {
                hand->selectedCard++;
            }
            break;

        case INPUT_LEFT:
            if (hand->selectedCard > 0) {
                hand->selectedCard--;
            }
            break;

        case INPUT_ENTER:
            if (estado->partida.suaVez && estado->jogador.qtdCartas > 0) {
                int cartaId = estado->jogador.cartas[hand->selectedCard].id;
                enviarSolicitacao(socket, jogadorId, ACAO_JOGAR_CARTA, cartaId);
            }
            break;

        case INPUT_DOWN:
            if (estado->partida.suaVez) {
                enviarSolicitacao(socket, jogadorId, ACAO_COMPRAR_CARTA, 0);
            }
            break;

        case INPUT_UP:
            enviarSolicitacao(socket, jogadorId, ACAO_DIZER_UNO, 0);
            break;

        case INPUT_ESCAPE:
            *running = 0;
            break;

        default:
            break;
    }
}


static void exibirResultadoFinal(
    bool connectionLost,
    bool adversarioSaiu,
    bool partidaFinalizada,
    bool venceu,
    bool perdeu
) {
    printf("\033[?25h\n");

    if (connectionLost) {
        printf("Conexao com o servidor perdida.\n");
        return;
    }

    if (adversarioSaiu) {
        printf("O adversario saiu da partida.\n");
        Sleep(2000);
        return;
    }

    if (partidaFinalizada) {
        if (venceu) {
            printf("Voce venceu!\n");
        } else if (perdeu) {
            printf("Voce perdeu!\n");
        } else {
            printf("Partida finalizada!\n");
        }

        fflush(stdout);
        Sleep(2000);
    }
}

int main(int argc, char *argv[]) {
    static char stdoutBuffer[STDOUT_BUFFER_SIZE];
    setvbuf(stdout, stdoutBuffer, _IOFBF, sizeof(stdoutBuffer));

    const char *serverIp = argc > 1 ? argv[1] : DEFAULT_SERVER_IP;
    int serverPort = argc > 2 ? atoi(argv[2]) : DEFAULT_SERVER_PORT;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    enableAnsiConsole();

    memset(&g_shared, 0, sizeof(g_shared));
    InitializeCriticalSection(&g_shared.lock);
    InitializeCriticalSection(&g_logLock);

    remove(DEBUG_LOG_FILE);
    debugLog("=== client iniciado (pid=%lu) ===", (unsigned long) GetCurrentProcessId());

    if (!iniciarWinsock()) {
        DeleteCriticalSection(&g_shared.lock);
        DeleteCriticalSection(&g_logLock);
        return EXIT_FAILURE;
    }

    SOCKET clientSocket = conectarServidor(serverIp, serverPort);

    if (clientSocket == INVALID_SOCKET) {
        WSACleanup();
        DeleteCriticalSection(&g_shared.lock);
        DeleteCriticalSection(&g_logLock);
        return EXIT_FAILURE;
    }

    if (!receberMensagemInicial(clientSocket)) {
        closesocket(clientSocket);
        WSACleanup();
        DeleteCriticalSection(&g_shared.lock);
        DeleteCriticalSection(&g_logLock);
        return EXIT_FAILURE;
    }

    HANDLE recvThread = CreateThread(
        NULL,
        0,
        recvThreadProc,
        (LPVOID) (uintptr_t) clientSocket,
        0,
        NULL
    );

    if (recvThread == NULL) {
        printf("Erro ao criar thread de recebimento.\n");
        closesocket(clientSocket);
        WSACleanup();
        DeleteCriticalSection(&g_shared.lock);
        DeleteCriticalSection(&g_logLock);
        return EXIT_FAILURE;
    }

    Screen screen = getScreenSize();

    if (!validarTela(&screen)) {
        shutdown(clientSocket, SD_BOTH);
        WaitForSingleObject(recvThread, INFINITE);
        CloseHandle(recvThread);
        closesocket(clientSocket);
        WSACleanup();
        freeScreen(&screen);
        DeleteCriticalSection(&g_shared.lock);
        DeleteCriticalSection(&g_logLock);
        return EXIT_FAILURE;
    }

    BoardLayout layout = createBoardLayout(screen);
    HandUI hand = {
        .zone = layout.bottom,
        .selectedCard = 0
    };

    int jogadorId = -1;
    int running = 1;

    printf("\033[2J\033[H\033[?25l");
    fflush(stdout);

    while (running) {
        EstadoJogo estado;
        bool hasEstado;
        bool connectionLost;
        bool partidaFinalizada;
        bool adversarioSaiu;
        char statusMsg[128];
        DWORD now = GetTickCount();

        EnterCriticalSection(&g_shared.lock);

        estado = g_shared.estado;
        hasEstado = g_shared.hasEstado;
        connectionLost = g_shared.connectionLost;
        partidaFinalizada = g_shared.partidaFinalizada;
        adversarioSaiu = g_shared.adversarioSaiu;

        if (g_shared.statusMsg[0] != '\0' &&
            now > g_shared.statusMsgExpiry) {
            g_shared.statusMsg[0] = '\0';
        }

        strncpy(statusMsg, g_shared.statusMsg, sizeof(statusMsg) - 1);
        statusMsg[sizeof(statusMsg) - 1] = '\0';

        LeaveCriticalSection(&g_shared.lock);

        if (hasEstado) {
            jogadorId = estado.jogador.id;
        }

        clearScreen(&screen);
        drawBoardBorder(&screen);
        drawBoard(&screen, layout);

        if (!hasEstado) {
            drawWaitingMessage(&screen, layout.center);
        } else {
            drawDiscardPile(&screen, layout.center, estado.partida.ultimaCarta);

            if (hand.selectedCard >= estado.jogador.qtdCartas) {
                hand.selectedCard = estado.jogador.qtdCartas > 0
                                        ? estado.jogador.qtdCartas - 1
                                        : 0;
            }

            drawPlayerHand(&screen, &hand, &estado.jogador);
            drawHUD(&screen, layout.top, &estado, statusMsg);
        }

        renderScreen(&screen);

        if (connectionLost || adversarioSaiu || partidaFinalizada) {
            break;
        }

        Input input = readInput();

        if (hasEstado) {
            processarEntrada(
                clientSocket,
                jogadorId,
                &estado,
                &hand,
                input,
                &running
            );
        } else if (input == INPUT_ESCAPE) {
            running = 0;
        }

        Sleep(30);
    }

    EnterCriticalSection(&g_shared.lock);
    bool connectionLost = g_shared.connectionLost;
    bool adversarioSaiu = g_shared.adversarioSaiu;
    bool partidaFinalizada = g_shared.partidaFinalizada;
    bool venceu = g_shared.hasEstado && g_shared.estado.jogador.qtdCartas == 0;
    bool perdeu = g_shared.hasEstado &&
                  g_shared.estado.partida.numeroCartasAdversario == 0;
    LeaveCriticalSection(&g_shared.lock);

    shutdown(clientSocket, SD_BOTH);
    WaitForSingleObject(recvThread, INFINITE);
    CloseHandle(recvThread);

    closesocket(clientSocket);
    WSACleanup();
    freeScreen(&screen);

    debugLog("=== client encerrado ===");

    DeleteCriticalSection(&g_shared.lock);
    DeleteCriticalSection(&g_logLock);

    exibirResultadoFinal(
        connectionLost,
        adversarioSaiu,
        partidaFinalizada,
        venceu,
        perdeu
    );

    printf("Encerrado. Pressione ENTER para sair...\n");
    fflush(stdout);
    getchar();

    return EXIT_SUCCESS;
}
