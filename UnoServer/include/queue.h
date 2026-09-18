#ifndef QUEUE_H
#define QUEUE_H

#include <windows.h>

#include "protocol.h"

#define QUEUE_CAPACITY 32

typedef struct {
    Solicitacao items[QUEUE_CAPACITY];

    int front;
    int rear;
    int count;

    int shutdown;

    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE notEmpty;
    CONDITION_VARIABLE notFull;

} RequestQueue;

typedef struct {
    Mensagem items[QUEUE_CAPACITY];

    int front;
    int rear;
    int count;

    int shutdown;

    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE notEmpty;
    CONDITION_VARIABLE notFull;

} ResponseQueue;


/* Request Queue */

int requestQueueInit(RequestQueue *queue);
void requestQueueDestroy(RequestQueue *queue);

int requestQueuePush(RequestQueue *queue, Solicitacao request);

int requestQueuePop(RequestQueue *queue, Solicitacao *request);

void requestQueueShutdown(RequestQueue *queue);


/* Response Queue */

int responseQueueInit(ResponseQueue *queue);
void responseQueueDestroy(ResponseQueue *queue);

int responseQueuePush(ResponseQueue *queue, Mensagem response);

int responseQueuePop(ResponseQueue *queue, Mensagem *response);

void responseQueueShutdown(ResponseQueue *queue);

#endif