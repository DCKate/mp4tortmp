//
//  kqueue.hpp
//  ForkProcess
//
//  Created by kateh on 2017/3/9.
//  Copyright © 2017年 FAKE. All rights reserved.
//

#ifndef kqueue_hpp
#define kqueue_hpp

#include <stdio.h>
#include <pthread.h>

typedef struct knode knode;
typedef struct kqueue kqueue;

struct knode{
    knode* next;
    char* data;
    int type;
    int len;
};

struct kqueue{
    pthread_mutex_t lock;
    knode* pop;
    knode* push;
    int count;
};

void push_kqueue(kqueue* qu, char* data,int type,int dlen);
int pop_kqueue(kqueue* qu, char* out);
void clear_kqueue(kqueue* qu);
knode* pop_kqueue_node(kqueue* qu);
void Free_pop_node(knode* no);
kqueue* init_kqueue();

#endif /* kqueue_hpp */
