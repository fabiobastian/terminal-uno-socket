#include "../include/queue.h"

/* ============================================================
 * REQUEST QUEUE
 * ============================================================ */

int requestQueueInit(RequestQueue *queue)
{
    if (queue == NULL) {
        return -1;
    }

    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->shutdown = 0;

    InitializeCriticalSection(&queue->mutex);

    InitializeConditionVariable(
        &queue->notEmpty
    );

    InitializeConditionVariable(
        &queue->notFull
    );

    return 0;
}


void requestQueueDestroy(RequestQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    DeleteCriticalSection(&queue->mutex);
}


int requestQueuePush(
    RequestQueue *queue,
    Solicitacao request
)
{
    if (queue == NULL) {
        return -1;
    }

    EnterCriticalSection(&queue->mutex);

    while (
        queue->count == QUEUE_CAPACITY &&
        !queue->shutdown
    ) {
        SleepConditionVariableCS(
            &queue->notFull,
            &queue->mutex,
            INFINITE
        );
    }

    if (queue->shutdown) {
        LeaveCriticalSection(&queue->mutex);
        return -1;
    }

    queue->items[queue->rear] = request;

    queue->rear =
        (queue->rear + 1) % QUEUE_CAPACITY;

    queue->count++;

    WakeConditionVariable(
        &queue->notEmpty
    );

    LeaveCriticalSection(&queue->mutex);

    return 0;
}


int requestQueuePop(
    RequestQueue *queue,
    Solicitacao *request
)
{
    if (
        queue == NULL ||
        request == NULL
    ) {
        return -1;
    }

    EnterCriticalSection(&queue->mutex);

    while (
        queue->count == 0 &&
        !queue->shutdown
    ) {
        SleepConditionVariableCS(
            &queue->notEmpty,
            &queue->mutex,
            INFINITE
        );
    }

    if (
        queue->count == 0 &&
        queue->shutdown
    ) {
        LeaveCriticalSection(&queue->mutex);
        return -1;
    }

    *request = queue->items[queue->front];

    queue->front =
        (queue->front + 1) % QUEUE_CAPACITY;

    queue->count--;

    WakeConditionVariable(
        &queue->notFull
    );

    LeaveCriticalSection(&queue->mutex);

    return 0;
}


void requestQueueShutdown(
    RequestQueue *queue
)
{
    if (queue == NULL) {
        return;
    }

    EnterCriticalSection(&queue->mutex);

    queue->shutdown = 1;

    WakeAllConditionVariable(
        &queue->notEmpty
    );

    WakeAllConditionVariable(
        &queue->notFull
    );

    LeaveCriticalSection(&queue->mutex);
}


/* ============================================================
 * RESPONSE QUEUE
 * ============================================================ */

int responseQueueInit(ResponseQueue *queue)
{
    if (queue == NULL) {
        return -1;
    }

    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->shutdown = 0;

    InitializeCriticalSection(&queue->mutex);

    InitializeConditionVariable(
        &queue->notEmpty
    );

    InitializeConditionVariable(
        &queue->notFull
    );

    return 0;
}


void responseQueueDestroy(ResponseQueue *queue)
{
    if (queue == NULL) {
        return;
    }

    DeleteCriticalSection(&queue->mutex);
}


int responseQueuePush(
    ResponseQueue *queue,
    Mensagem response
)
{
    if (queue == NULL) {
        return -1;
    }

    EnterCriticalSection(&queue->mutex);

    while (
        queue->count == QUEUE_CAPACITY &&
        !queue->shutdown
    ) {
        SleepConditionVariableCS(
            &queue->notFull,
            &queue->mutex,
            INFINITE
        );
    }

    if (queue->shutdown) {
        LeaveCriticalSection(&queue->mutex);
        return -1;
    }

    queue->items[queue->rear] = response;

    queue->rear =
        (queue->rear + 1) % QUEUE_CAPACITY;

    queue->count++;

    WakeConditionVariable(
        &queue->notEmpty
    );

    LeaveCriticalSection(&queue->mutex);

    return 0;
}


int responseQueuePop(
    ResponseQueue *queue,
    Mensagem *response
)
{
    if (
        queue == NULL ||
        response == NULL
    ) {
        return -1;
    }

    EnterCriticalSection(&queue->mutex);

    while (
        queue->count == 0 &&
        !queue->shutdown
    ) {
        SleepConditionVariableCS(
            &queue->notEmpty,
            &queue->mutex,
            INFINITE
        );
    }

    if (
        queue->count == 0 &&
        queue->shutdown
    ) {
        LeaveCriticalSection(&queue->mutex);
        return -1;
    }

    *response = queue->items[queue->front];

    queue->front =
        (queue->front + 1) % QUEUE_CAPACITY;

    queue->count--;

    WakeConditionVariable(
        &queue->notFull
    );

    LeaveCriticalSection(&queue->mutex);

    return 0;
}


void responseQueueShutdown(
    ResponseQueue *queue
)
{
    if (queue == NULL) {
        return;
    }

    EnterCriticalSection(&queue->mutex);

    queue->shutdown = 1;

    WakeAllConditionVariable(
        &queue->notEmpty
    );

    WakeAllConditionVariable(
        &queue->notFull
    );

    LeaveCriticalSection(&queue->mutex);
}