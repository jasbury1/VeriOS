/**
 * 
 * HEADER GOES HERE- make it look cool later
 *
 *
 *
 *
 */


/*  DEFINE A TCB HERE!  */
/* (Task control block) */

void OS_task_create(TaskFunc_t task_func, void *task_arg, 
      const char *task_name);
/*
 * The static counterpart to OS_task_create
 */
void OS_task_create_s();

void OS_task_destroy();

void OS_task_change_priority();

void OS_task_suspend();

void OS_task_resume();


