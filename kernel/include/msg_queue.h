#ifndef OS_MSG_QUEUE_H
#define OS_MSG_QUEUE_H

#include "verios.h"

/*******************************************************************************
* MACROS
*******************************************************************************/

#define OS_MSG_POOL_INITIAL_SIZE 8

/*******************************************************************************
* TYPEDEFS AND DATA STRUCTURES
*******************************************************************************/

/* The handle for application usage */
typedef void * MsgQueue_t;

typedef struct OSMessage Message_t;

struct OSMessage {
    TCB_t *sender;
    void *contents;

    Message_t *next_ptr;
};

typedef struct OSMessageQueue {
    int max_messages;
    int num_messages;

    WaitList_t send_waiters;
    WaitList_t reveive_waiters;

    Message_t *head_ptr;
    Message_t *tail_ptr;

    portMUX_TYPE mux;
} MessageQueue_t;

typedef struct OSMessagePool {
    int num_messages;
    Message_t *head_ptr;
    Message_t *tail_ptr;
} MessagePool_t;

/*******************************************************************************
* FUNCTION HEADERS
*******************************************************************************/

int OS_msg_queue_send(MsgQueue_t queue, TickType_t timeout, const void * const data);

int OS_msg_queue_receive(MsgQueue_t queue, TickType_t timeout, void ** msg);

void _OS_msg_queue_init(MessageQueue_t * queue, int queue_size);

int OS_msg_queue_create(MsgQueue_t * queue_ptr, int queue_size);

int OS_msg_queue_try_send(MsgQueue_t queue, const void * const data);

int OS_msg_queue_try_receive(MsgQueue_t queue, void ** data);

#endif /* OS_MSG_QUEUE_H */