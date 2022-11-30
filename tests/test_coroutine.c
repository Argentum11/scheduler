#include <stdio.h>
#include "../coroutine.h"
// COROUTINE_DEFINE is what a task will do while executing
COROUTINE_DEFINE(job)
{
    VAR_DEFINE2(int, i, j);
    VAR_DEFINE3(double, k, l, m);
    ARRAY_DEFINE(int, arr, 20);
    cr_begin();
    cr_set(i, 1);
    cr_set(j, 2);
    cr_set(k, 2.2);
    
    cr_set(arr, 2, 4 /* index */);
    printf("[@ job %d] %d %d\n", *(int *)args, cr_dref(i), cr_dref(j));

    cr_yield();

    cr_set(i, cr_dref(i) + 1);
    if (cr_dref(arr, 4 /* index */) == 2)
        printf("array success\n");
    printf("[# job %d] %d %d\n", *(int *)args, cr_dref(i), cr_dref(j));
    if (cr_dref(k) == 2.2)
        printf("variable success\n");

    cr_end();
}

int main(void)
{
    int crfd, tfd[10];
    printf("1...defalt/2...FIFO/4...LIFO:  ");
    int choice;
    scanf("%d", &choice);
    int scheduler;
    if (choice == 2 || choice == 4) {
        scheduler = choice;
    }
    else {
        scheduler = CR_DEFAULT;
    }
    crfd = coroutine_create(scheduler);
    if (crfd < 0)
        return crfd;

    for (int i = 0; i < 10; i++) {
        tfd[i] = i;
        printf("[tfd %d] %d added, %d\n", coroutine_add(crfd, job, &tfd[i]), i,
               tfd[i]);
    }

    coroutine_start(crfd);

    coroutine_join(crfd);
    return 0;
}
