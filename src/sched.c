#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <stdio.h>

#include "rbtree.h"
#include "context.h"
#include "coroutine.h"
#include "coroutine_int.h"

/* FIFO scheduler */

static inline int fifo_schedule(struct cr *cr, job_t func, void *args)
{
    struct task_struct *new_task; // 宣告變數

    new_task = calloc(1, sizeof(struct task_struct)); // 設定 new task
    if (!new_task)
        return -ENOMEM;

    /* 需要進入 runqueue 的 queue 中 task 數量 <0 = >釋放 new_task 並回傳 out of memory 狀態*/
    if (rq_enqueue(&cr->rq, new_task) < 0)
    {
        free(new_task);
        return -ENOMEM;
    }

    new_task->cr = cr;
    new_task->tfd = cr->size++; // task 的 tfd 為 task 的數量加1
    new_task->job = func;       // 工作為傳入之 func
    new_task->args = args;
    new_task->context.label = NULL;   // new_task 的 context label 陣列為空
    new_task->context.wait_yield = 1; // 沒有在等待或中斷
    new_task->context.blocked = 1;    // 沒有呼叫 cr_to_proce 且為預設

    return new_task->tfd; // 回傳新 task 的 tfd
}

static inline int lifo_schedule(struct cr *cr, job_t func, void *args)
{
    struct task_struct *new_task; // 宣告變數

    new_task = calloc(1, sizeof(struct task_struct)); // 設定 new task
    if (!new_task)
        return -ENOMEM;

    /* 需要進入 runqueue 的 queue 中 task 數量 <0 = >釋放 new_task 並回傳 out of memory 狀態*/
    if (rq_enqueue(&cr->rq, new_task) < 0)
    {
        free(new_task);
        return -ENOMEM;
    }

    new_task->cr = cr;
    new_task->tfd = cr->size++; // task 的 tfd 為 task 的數量加1
    new_task->job = func;       // 工作為傳入之 func
    new_task->args = args;
    new_task->context.label = NULL;   // new_task 的 context label 陣列為空
    new_task->context.wait_yield = 1; // 沒有在等待或中斷
    new_task->context.blocked = 1;    // 沒有呼叫 cr_to_proce 且為預設

    return new_task->tfd; // 回傳新 task 的 tfd
}

/* 把 task 從 runqueue 中移除 */
static inline struct task_struct *fifo_pick_next_task(struct cr *cr)
{
    return rq_dequeue(&cr->rq);
}

/* 把 task 從 runqueue 中移除 */
static inline int fifo_put_prev_task(struct cr *cr, struct task_struct *prev)
{
    return rq_enqueue(&cr->rq, prev);
}

/* 把 task 從 runqueue 中移除 */
static inline struct task_struct *lifo_pick_next_task(struct cr *cr)
{
    return rq_dequeue_lifo(&cr->rq);
}

/* 把 task 從 runqueue 中移除 */
static inline int lifo_put_prev_task(struct cr *cr, struct task_struct *prev)
{
    return rq_enqueue(&cr->rq, prev);
}

/* Default scheduler */
/* 紅黑樹排序插入定義 */
static RBTREE_CMP_INSERT_DEFINE(rb_cmp_insert, _n1, _n2)
{
    /* 設定變數 */
    struct task_struct *n1 = container_of(_n1, struct task_struct, node);
    struct task_struct *n2 = container_of(_n2, struct task_struct, node);

    if (n1->sum_exec_runtime < n2->sum_exec_runtime) // n1 執行的總時間 < n2 執行的總時間
        return 1;                                    // 回傳1
    else
    {
        if (n1->sum_exec_runtime == n2->sum_exec_runtime) // n1 執行的總時間 < n2 執行的總時間
            n1->sum_exec_runtime++;                       // n1 總執行時間加1
        return 0;                                         // 回傳0
    }
}

/* 紅黑樹排序尋找定義 */
static RBTREE_CMP_SEARCH_DEFINE(rb_cmp_search, _n1, key)
{
    struct task_struct *n1 = container_of(_n1, struct task_struct, node); // 設定變數

    if (n1->sum_exec_runtime == *(long *)key) // n1 總執行時間等於 key
        return RB_EQUAL;
    else if (n1->sum_exec_runtime > *(long *)key) // n1 總執行時間大於 key
        return RB_RIGHT;
    else // n1 總執行時間小於 key
        return RB_LEFT;
}

/* 定義 time_diff */
#define time_diff(start, end) \
    (end - start < 0 ? (1000000000 + end - start) : (end - start))

static inline int default_schedule(struct cr *cr, job_t func, void *args)
{
    /* 設定變數 */
    struct task_struct *new_task;
    static long exec_base = 0;

    new_task = calloc(1, sizeof(struct task_struct));
    if (!new_task)      // new_task 為 0
        return -ENOMEM; // out of memory

    new_task->sum_exec_runtime = exec_base;                   // 這邊是0
    rbtree_insert(&cr->root, &new_task->node, rb_cmp_insert); // 將 new_task->node 插入 cr->root

    new_task->cr = cr;
    new_task->tfd = cr->size++; // new_task 的 tfd 為 cr 中 task 的數量, task 數量加1
    new_task->job = func;       // 工作為傳入的 func
    new_task->args = args;
    new_task->context.label = NULL;   // new_task 的 context label 陣列為空
    new_task->context.wait_yield = 1; // 沒有在等待或中斷
    new_task->context.blocked = 1;    // 沒有呼叫 cr_to_proce 且為預設

    return new_task->tfd; // 回傳 new_task 的 tfd
}

/* 抓取下一個 task 執行並把 task 從 runqueue 和 rbtree 中移除 */
static inline struct task_struct *default_pick_next_task(struct cr *cr)
{
    struct rb_node *node = rbtree_min(&cr->root);                            // node 為 cr->root 的最小紅黑樹
    struct task_struct *task = container_of(node, struct task_struct, node); // 設定 task
    struct timespec start;                                                   // 宣告變數

    if (node == NULL) // 節點為空 => 回傳 NULL
        return NULL;
    __rbtree_delete(&cr->root, node);       // 刪除 node
    clock_gettime(CLOCK_MONOTONIC, &start); // 擷取時間
    task->exec_start = start.tv_nsec;       // task 的 exec_start 為 start 的時間(秒)

    return task; // 回傳 task
}

static inline int default_put_prev_task(struct cr *cr, struct task_struct *prev)
{
    struct timespec end; // 宣告變數

    clock_gettime(CLOCK_MONOTONIC, &end);                               // 獲取時間
    prev->sum_exec_runtime += time_diff(prev->exec_start, end.tv_nsec); // prev 的總執行時間 + [結束時間-開始時間] (+1000000000)
    rbtree_insert(&cr->root, &prev->node, rb_cmp_insert);               // 將 prev->node 插入 root 的紅黑樹

    return 0;
}

/* 初始化 scheduler */
void sched_init(struct cr *cr)
{
    switch (cr->flags)
    {
    case CR_DEFAULT:                                 // default
        printf("default\n");
        RB_ROOT_INIT(cr->root);                      // 初始化 root (cnt=0 父黑 左右子null)
        cr->schedule = default_schedule;             // 使用 default 的排程法
        cr->pick_next_task = default_pick_next_task; // 抓取下一個task執行並把task從runqueue和rbtree中移除
        cr->put_prev_task = default_put_prev_task;   // 儲存計算好的sum_exec_runtime並把current job在runqueue新增並插入rbtree中以便之後resume
        return;
        
    case CR_FIFO:     
        printf("fifo\n");                         // fifo
        rq_init(&cr->rq);                         // runqueue 初始化
        cr->schedule = fifo_schedule;             // 使用 fifo 的排程法
        cr->pick_next_task = fifo_pick_next_task; // 把 task 從 runqueue 中移除
        cr->put_prev_task = fifo_put_prev_task;   //把 task 加入 runqueue
        break;

    case CR_LIFO:
        printf("lifo\n");                         // lifo
        rq_init(&cr->rq);                         // requeue 初始化
        cr->schedule = lifo_schedule;             // 使用 lifo 的排程法
        cr->pick_next_task = lifo_pick_next_task; // 把 task 從 runqueue 中移除
        cr->put_prev_task = lifo_put_prev_task;   //把 task 加入 runqueue
    }
}
