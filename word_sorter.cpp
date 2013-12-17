#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include "MyTimer.h"

#define MAXLINE 32
#define IN_THREAD_MERGE

HANDLE g_mutex = NULL;

struct word
{
    char data[MAXLINE];
};

struct node
{
    char *vp;//value pointer
    node *next;
};

struct slice
{
    node *n_list;
    slice *next;
};

struct argvlist
{
    slice sl;
    int words;
};

slice *g_list = NULL;
slice *g_tail = NULL;

void swap_vp(node *a, node *b)
{
    char *tmp;
    tmp = a->vp;
    a->vp = b->vp;
    b->vp = tmp;
}

void quick_sort(node *n_arr, int begin, int end)
{
    if (begin >= end) return;

    node *item = &n_arr[begin];
    int low = begin, high = end;

    while (low < high)
    {
        while (low < high && strcmp(item->vp, n_arr[high].vp) <= 0) high--;
        while (low < high && strcmp(item->vp, n_arr[low].vp) >= 0) low++;
        swap_vp(&n_arr[low], &n_arr[high]);
    }
    swap_vp(&n_arr[low], item);

    quick_sort(n_arr, begin, low - 1);
    quick_sort(n_arr, low + 1, end);
}

node *merge(node *left, node *right)
{
    node *res = NULL, *prev = NULL;

    if (strcmp(left->vp, right->vp) <= 0)
    {
        res = prev = left;
        left = left->next;
    }
    else
    {
        res = prev = right;
        right = right->next;
    }

    while (left != NULL && right != NULL)
    {
        if (strcmp(left->vp, right->vp) <= 0)
        {
            prev->next = left;
            prev = left;
            left = left->next;
        }
        else
        {
            prev->next = right;
            prev = right;
            right = right->next;
        }
    }

    if (left != NULL)
        prev->next = left;
    else
        prev->next = right;

    return res;
}

slice *get_slice()
{
    slice *ret = NULL;

    WaitForSingleObject(g_mutex, INFINITE);
    if (g_list != NULL)
    {
        ret = g_list;
        g_list = ret->next;
        ret->next = NULL;
    }
    ReleaseMutex(g_mutex);

    return ret;
}

void insert_slice(slice *s)
{
    WaitForSingleObject(g_mutex, INFINITE);
    if (g_list == NULL)
    {
        g_list = s;
        g_tail = s;
    }
    else
    {
        g_tail->next = s;
        g_tail = s;
    }
    ReleaseMutex(g_mutex);
}

void _cdecl thread_worker(void *ptr)
{
    quick_sort(((argvlist *)ptr)->sl.n_list, 0, ((argvlist *)ptr)->words - 1);

#ifdef IN_THREAD_MERGE
    slice *s_a = NULL;
    node *n_m = NULL;

    s_a = get_slice();
    if (s_a != NULL)
    {
        n_m = merge(s_a->n_list, ((argvlist *)ptr)->sl.n_list);
        s_a->n_list = n_m;
        insert_slice(s_a);
    }
    else
#endif
        insert_slice(&((argvlist *)ptr)->sl);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("WordSorter C++ implementation by lenie\n"\
               "usage: %s threadcount inputfile outputfile\n"\
               "e.g. : %s 4 sowpods.txt out.txt\n",
               argv[0], argv[0]);

        exit(-1);
    }

    int thread_num, word_num;
    thread_num = atoi(argv[1]);

    FILE *infile_ptr = NULL;
    FILE *outfile_ptr = NULL;

    infile_ptr = fopen(argv[2], "r+");
    outfile_ptr = fopen(argv[3], "w+");

    // Timer
    MyTimer mt;
    int total_time = 0;

    printf("reading file... ");
    mt.Start();

    // read count
    char count[MAXLINE];
    if (fgets(count, MAXLINE, infile_ptr) != NULL)
    {
        word_num = atoi(count);
    }

    word *w_arr;
    w_arr = (word *)calloc(word_num, sizeof(word));

    node *n_arr, *n_head, *n_cur;
    n_arr = (node *)calloc(word_num, sizeof(node));
    n_head = n_arr;

    // read words
    int i;
    for (i = 0; fgets(w_arr[i].data, MAXLINE, infile_ptr) != NULL; i++)
    {
        n_arr[i].vp = w_arr[i].data;
        n_arr[i].next = &n_arr[i + 1];
    }
    n_arr[i - 1].next = NULL;

    fclose(infile_ptr);

    mt.Stop();
    printf("%d ms\n", (int)mt.d_costTime);
    total_time += (int)mt.d_costTime;

    // multi-thread sort
    printf("%d threads sorting... ", thread_num);
    mt.Start();
    g_mutex = CreateMutex(NULL, FALSE, NULL);

    HANDLE *thread_arr;
    thread_arr = (HANDLE *)calloc(thread_num, sizeof(HANDLE));

    int words_per_thread, words_remain, words_for_thread;

    words_per_thread = word_num / thread_num;
    words_remain = word_num % thread_num;

    argvlist *al_arr;
    al_arr = (argvlist *)calloc(thread_num, sizeof(argvlist));

    node *n_offset = n_head;

    for (i = 0; i < thread_num; i++)
    {
        words_for_thread = words_per_thread;

        if (words_remain > 0)
        {
            words_for_thread++;
            words_remain--;
        }

        al_arr[i].sl.n_list = n_offset;
        al_arr[i].words = words_for_thread;

        //set tail node's ptr
        n_offset += words_for_thread - 1;
        n_offset->next = NULL;
        n_offset++;

        thread_arr[i] = (HANDLE)_beginthread(thread_worker, 0, &al_arr[i]);
    }

    for (i = 0; i < thread_num; i++)
    {
        WaitForSingleObject(thread_arr[i], INFINITE);
    }
    mt.Stop();
    printf("%d ms\n", (int)mt.d_costTime);
    total_time += (int)mt.d_costTime;

    // merge remain slices
    printf("merging slices... ");
    slice *s_one = NULL, *s_two = NULL;
    node *m = NULL;
    while (1)
    {
        if (g_list == NULL) break;
        
        if (s_one == NULL)
        {
            s_one = get_slice();
            continue;
        }
        else
        {
            s_two = get_slice();

            if (s_one != NULL && s_two != NULL)
            {
                m = merge(s_one->n_list, s_two->n_list);
                s_one->n_list = m;
                n_head = m;
                insert_slice(s_one);
                s_one = s_two = NULL;
            }
        }
    }

    mt.Stop();
    printf("%d ms\n", (int)mt.d_costTime);
    total_time += (int)mt.d_costTime;

    // write to file
    printf("writing to file... ");
    mt.Start();
    fputs(count, outfile_ptr);
    n_cur = n_head;
    while (1)
    {
        fputs(n_cur->vp, outfile_ptr);
        if (n_cur->next == NULL) break;
        n_cur = n_cur->next;
    }
    fclose(outfile_ptr);

    mt.Stop();
    printf("%d ms\n", (int)mt.d_costTime);
    total_time += (int)mt.d_costTime;

    printf("Total time: %d ms\n", total_time);

    free(w_arr);
    free(n_arr);
    free(thread_arr);
    free(al_arr);

    return 0;
}
