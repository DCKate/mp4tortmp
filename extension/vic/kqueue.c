//
//  kqueue.cpp
//  ForkProcess
//
//  Created by kateh on 2017/3/9.
//  Copyright © 2017年 FAKE. All rights reserved.
//

#include "kqueue.h"
#include <stdlib.h>
#include <string.h>
void push_kqueue(kqueue* qu, char* data,int type,int dlen){
    knode* tmp = (knode*)malloc(sizeof(knode));
    tmp->data = (char*)malloc(dlen);
    tmp->next = NULL;
    memcpy(tmp->data,data,dlen);
    tmp->len = dlen;
    tmp->type = type;
    pthread_mutex_lock(&qu->lock);
    if (qu->push) {
        knode* pre = qu->push;
        pre->next = tmp;
    }
    qu->push = tmp;
    if (qu->pop==NULL) {
        qu->pop = qu->push;
    }
    qu->count++;
    //printf("push qu num %d\n", qu->count);
    pthread_mutex_unlock(&qu->lock);
}

int pop_kqueue(kqueue* qu, char* out){
    int rlen = 0;
    pthread_mutex_lock(&qu->lock);
    if(qu->pop){
        knode* tmp = qu->pop;
        if (tmp->next==NULL) {
            qu->pop=NULL;
            qu->push=NULL;
        }else{
            qu->pop = tmp->next;
        }
        qu->count--;
        tmp->next = NULL;
        memcpy(out, tmp->data, tmp->len);
        free(tmp->data);
        free(tmp);
        rlen = tmp->len;
    }
    pthread_mutex_unlock(&qu->lock);
    return rlen;
}

knode* pop_kqueue_node(kqueue* qu){
    int rlen = 0;
    knode* tt = NULL;
    pthread_mutex_lock(&qu->lock);
    if(qu->pop){
        knode* tmp = qu->pop;
        if (tmp->next==NULL) {
            qu->pop=NULL;
            qu->push=NULL;
        }else{
            qu->pop = tmp->next;
        }
        qu->count--;
        tmp->next = NULL;
        tt = tmp;
    }
    //printf("pop qu num %d\n", qu->count);
    pthread_mutex_unlock(&qu->lock);
    return tt;
}

void Free_pop_node(knode* no){
    if(no){
        free(no->data);
        free(no);
        no = NULL;
    }
}

void clear_kqueue(kqueue* qu){
    pthread_mutex_lock(&qu->lock);
    knode* tmp = qu->pop;
    while (tmp) {
        knode* ftmp = tmp;
        tmp = tmp->next;
        
        if (ftmp->next==NULL) {
            qu->pop=NULL;
            qu->push=NULL;
        }
        free(ftmp->data);
        free(ftmp);
    }
    pthread_mutex_unlock(&qu->lock);
    pthread_mutex_destroy(&qu->lock);
    free(qu);

}

kqueue* init_kqueue(){
    kqueue* qu = (kqueue*)malloc(sizeof(kqueue));
    int ret = pthread_mutex_init(&qu->lock, NULL);
    if (ret != 0) {
        printf("Mutex_init for Kalay Queue failed.\n");
        free(qu);
        return NULL;
    }
    qu->pop = NULL;
    qu->push = NULL;
    qu->count = 0;
    return qu;
}
