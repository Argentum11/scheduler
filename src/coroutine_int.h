#ifndef __COROUTINE_INT_H__
#define __COROUTINE_INT_H__

#include "rbtree.h"
#include "context.h"

typedef int (*job_t)(struct context *__context, void *args);

/*
 * task_struct maintain the coroutine or task object.
 */
struct task_struct
{
    /* job information */
    struct cr *cr;          // coroutine
    int tfd;                /* task fd */
    job_t job;              // job
    void *args;             // args
    struct context context; /* defined at context.h */

    /* default info */
    struct
    {
        struct rb_node node;   // 紅黑樹節點
        long sum_exec_runtime; // 總執行時間
        long exec_start;       // 開始執行時間
    };
};

/* 定義container_of 並防止使用延展功能時gcc提出警告*/
#ifndef container_of
#define container_of(ptr, type, member)                        \
    __extension__({                                            \
        const __typeof__(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member));     \
    })
#endif

/* task_of 主要以 __context、task_srtuct 及 context 執行 container_of*/
#define task_of(__context) container_of(__context, struct task_struct, context)

/* runqueue */

// Need to be power of two
#define RINGBUFFER_SIZE 16

struct rq
{
    unsigned int out, in;                   /* dequeue at out, enqueue  at in*/
    unsigned int mask;                      /* the size is power of two, so mask will be size - 1 */
    struct task_struct *r[RINGBUFFER_SIZE]; // runqueue 的陣列
};

void rq_init(struct rq *rq);                             // 初始化 runqueue
int rq_enqueue(struct rq *rq, struct task_struct *task); // 進入 runqueue 的函式
struct task_struct *rq_dequeue(struct rq *rq);           // 離開 runqueue 的函式

struct task_struct *rq_dequeue_lifo(struct rq *rq); // lifo 函式

/* main data structure */

#define MAX_CR_TABLE_SIZE 10 // corourine 的 table 最大大小為 10

struct cr
{
    unsigned long size;          /* number of the task in this scheduler */
    int crfd;                    /* coroutine fd number */
    int flags;                   /* Which type of scheduler, FIFO or CFS */
    struct task_struct *current; /* the job currently working */

    /* scheduler - chose by the flags */
    struct rq rq;        /* FIFO */
    struct rb_root root; /* Default */

    /* sched operations */
    int (*schedule)(struct cr *cr, job_t func, void *args);        // schedule 函式
    struct task_struct *(*pick_next_task)(struct cr *cr);          // 定義task_struct，其中含 cr 結構
    int (*put_prev_task)(struct cr *cr, struct task_struct *prev); // put_prev_task 函式
};

struct cr_struct
{
    int size;                            // 大小
    struct cr *table[MAX_CR_TABLE_SIZE]; // coroutine 的 table
};

void sched_init(struct cr *cr); // 初始化排程法的 function

#endif /* __COROUTINE_INT_H__ */