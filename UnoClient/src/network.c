#include "../include/network.h"

void debugLog(const char *fmt, ...);

int enviarTudo(SOCKET socket, const char *buffer, int tamanho) {
    int totalEnviado = 0;

    while (totalEnviado < tamanho) {
        int enviado = send(
            socket,
            buffer + totalEnviado,
            tamanho - totalEnviado,
            0
        );

        if (enviado == SOCKET_ERROR) return -1;

        totalEnviado += enviado;
    }

    return totalEnviado;
}

int recvAll(SOCKET socket, char *buffer, int tamanho) {
    int totalRecebido = 0;

    while (totalRecebido < tamanho) {
        int recebido = recv(
            socket,
            buffer + totalRecebido,
            tamanho - totalRecebido,
            0
        );

        if (recebido == 0) return 0;
        if (recebido == SOCKET_ERROR) return -1;

        totalRecebido += recebido;
    }

    return totalRecebido;
}

bool enviarSolicitacao(
    SOCKET socket,
    int jogadorId,
    TipoAcao acao,
    int cartaId
) {
    Solicitacao request = {
        .jogadorId = jogadorId,
        .acao = acao,
        .cartaId = cartaId
    };

    bool ok = enviarTudo(
                  socket,
                  (const char *) &request,
                  sizeof(request)
              ) == sizeof(request);

    debugLog(
        "ENVIADO -> jogadorId=%d acao=%d cartaId=%d ok=%d",
        jogadorId,
        (int) acao,
        cartaId,
        ok ? 1 : 0
    );

    return ok;
}
