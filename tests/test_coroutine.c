#include <stdio.h>
#include "../coroutine.h"
// COROUTINE_DEFINE is what a task will do while executing
COROUTINE_DEFINE(job) // 定義 coroutine
{
    VAR_DEFINE2(int, i, j);       // 定義 int variable
    VAR_DEFINE3(double, k, l, m); // 定義 double variable
    ARRAY_DEFINE(int, arr, 20);   // 定義 int 型態的 array 且大小為 20
    cr_begin();                   // 初始化 coroutine

    /*設定 coroutine*/
    cr_set(i, 1);
    cr_set(j, 2);
    cr_set(k, 2.2);

    cr_set(arr, 2, 4 /* index */);
    printf("[@ job %d] %d %d\n", *(int *)args, cr_dref(i), cr_dref(j)); // 輸出資訊

    cr_yield(); // 狀態設為 yield

    cr_set(i, cr_dref(i) + 1);            // 設定 coroutine
    if (cr_dref(arr, 4 /* index */) == 2) // index 中第4個內容為 2 => 顯示 array 設定成功
        printf("array success\n");
    printf("[# job %d] %d %d\n", *(int *)args, cr_dref(i), cr_dref(j)); // 輸出資訊
    if (cr_dref(k) == 2.2)                                              // cr_dref(k) 為 2.2 => 顯示變數設定成功
        printf("variable success\n");

    cr_end(); // 結束
}

int main(void)
{

    int crfd, tfd[10];                         // 宣告變數
    printf("1...defalt/2...FIFO/4...LIFO:  "); // 提示選擇
    int choice;                                // 選擇的變數
    scanf("&d", &choice);                      // 輸入選擇
    int scheduler;                             // scheduler 的變數
    /*判斷選擇*/
    if (choice == 2 || choice == 4)
    {
        scheduler = choice;
    }
    else
    {
        scheduler = CR_DEFAULT;
    }
    crfd = coroutine_create(scheduler); // crfd 為 scheduler 為基制定的 coroutine
    if (crfd < 0)                       // crfd 為 out of memory 或 bad address 的狀態 => 回傳狀態代號
        return crfd;

    for (int i = 0; i < 10; i++)
    {
        tfd[i] = i;                                                                      // tfd 陣列內容設定
        printf("[tfd %d] %d added, %d\n", coroutine_add(crfd, job, &tfd[i]), i, tfd[i]); // 輸出資訊
    }

    coroutine_start(crfd); // 依據不同 status 行動

    coroutine_join(crfd); // 釋放該 coroutine 記憶體，並將 coroutine table size -1

    return 0;
}
