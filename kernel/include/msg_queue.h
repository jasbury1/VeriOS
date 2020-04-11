#ifndef OS_MSG_QUEUE_H
#define OS_MSG_QUEUE_H

#include "verios.h"

/*******************************************************************************
* MACROS
*******************************************************************************/


/*******************************************************************************
* TYPEDEFS AND DATA STRUCTURES
*******************************************************************************/

typedef struct OSMessageQueue MessageQueue_t;
typedef struct OSTaskListHeader WaitList_t;

typedef struct OSMessage {

    TCB_t *sender;
    void *contents;

} Message_t;

struct OSMessageQueue {
    int max_messages;
    int num_messages;

    WaitList_t send_waiters;
    WaitList_t reveive_waiters;

    MessageQueue_t *head_ptr;
    MessageQueue_t *tail_ptr;

    portMUX_TYPE mux;
};

/*******************************************************************************
* FUNCTION HEADERS
*******************************************************************************/

void OS_msg_queue_init(MessageQueue_t *msg_queue, int queue_size);

#endif /* OS_MSG_QUEUE_H */