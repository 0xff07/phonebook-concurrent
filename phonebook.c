#define  _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

#include "list.h"
#include "text_align.h"
#include "debug.h"
#include <fcntl.h>
#define ALIGN_FILE "align.txt"
#define OUTPUT_FILE "opt.txt"
#define MAX_LAST_NAME_SIZE 16
#ifndef THREAD_NUM
#define THREAD_NUM 4
#endif

#define DICT_FILE "./dictionary/words.txt"

static double diff_in_second(struct timespec t1, struct timespec t2)
{
    struct timespec diff;
    if (t2.tv_nsec-t1.tv_nsec < 0) {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
    } else {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec + diff.tv_nsec / 1000000000.0);
}

typedef struct _thread_arg {
    void *map;
    llist_t *phonebook;
    int map_size;
    int no;
} thread_arg;

thread_arg* new_thread_arg(void *map, llist_t* phonebook, int map_size, int no)
{
    thread_arg *arg = malloc(sizeof(thread_arg));
    arg -> map = map;
    arg -> phonebook = phonebook;
    arg -> map_size = map_size;
    arg -> no = no;
    return arg;
}

void append_job(void* arg)
{
    thread_arg* a = (thread_arg*)arg;
    char *map = (char*)(a -> map);
    for (int i = a-> no; i < (a -> map_size) / MAX_LAST_NAME_SIZE; i+= THREAD_NUM) {
        list_add(a -> phonebook, (val_t)(map + i * MAX_LAST_NAME_SIZE));
    }
    pthread_exit(0);
}

int main(int argc, char *argv[])
{
    struct timespec mid;
    struct timespec start, end;
    double cpu_time1, cpu_time2;

    /* File preprocessing */
    text_align(DICT_FILE, ALIGN_FILE, MAX_LAST_NAME_SIZE);
    int fd = open(ALIGN_FILE, O_RDONLY | O_NONBLOCK);
    off_t file_size = fsize(ALIGN_FILE);
    pthread_t threads[THREAD_NUM];
    thread_arg *thread_args[THREAD_NUM];
    char *map;

    /* Start timing */
    clock_gettime(CLOCK_REALTIME, &start);
    /* Allocate the resource at first */
    map = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    assert(map && "mmap error");

    llist_t *phonebook = list_new();

    /* Prepare for multi-threading */
    pthread_setconcurrency(THREAD_NUM + 1);
    for (int i = 0; i < THREAD_NUM; i++)
        thread_args[i] = new_thread_arg(map, phonebook, file_size, i);
    /* Deliver the jobs to all threads and wait for completing */
    clock_gettime(CLOCK_REALTIME, &mid);

    for (int i = 0; i < THREAD_NUM; i++)
        pthread_create(&threads[i], NULL, (void*)append_job, (void *)thread_args[i]);

    for (int i = 0; i < THREAD_NUM; i++)
        pthread_join(threads[i], NULL);

    /* Stop timing */
    clock_gettime(CLOCK_REALTIME, &end);


    cpu_time1 = diff_in_second(start, end);

    /* Find the given entry */
    /* the givn last name to find */
    char input[MAX_LAST_NAME_SIZE] = "zyxel";

    node_t *left_node = NULL;
    assert(0 == strcmp((char*)(list_search(phonebook, (val_t)input, &left_node)->data), "zyxel"));

    /* Compute the execution time */
    clock_gettime(CLOCK_REALTIME, &start);
    list_search(phonebook, (val_t)input, &left_node);
    clock_gettime(CLOCK_REALTIME, &end);
    cpu_time2 = diff_in_second(start, end);

    /* Write the execution time to file. */
    FILE *output;
    output = fopen(OUTPUT_FILE, "a");
    fprintf(output, "append() list_search() %lf %lf\n", cpu_time1, cpu_time2);
    fclose(output);

    printf("execution time of append() : %lf sec\n", cpu_time1);
    printf("execution time of findName() : %lf sec\n", cpu_time2);

    /* Release memory */
    munmap(map, file_size);
    close(fd);

    return 0;
}
