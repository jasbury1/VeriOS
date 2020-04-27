#ifndef OS_MSG_QUEUE_H
#define OS_MSG_QUEUE_H

#include "verios.h"

/*******************************************************************************
* MACROS
*******************************************************************************/


/*******************************************************************************
* TYPEDEFS AND DATA STRUCTURES
*******************************************************************************/

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

int OS_msg_queue_post(MessageQueue_t *msg_queue, TickType_t timeout, const void * const data);

void _OS_msg_queue_init(MessageQueue_t *msg_queue, int queue_size);

int OS_msg_queue_create(void ** queue_ptr, int queue_size);

#endif /* OS_MSG_QUEUE_H */