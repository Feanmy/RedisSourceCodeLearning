/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * listRelease(), but private value of every node need to be freed
 * by the user before to call listRelease(), or by setting a free method using
 * listSetFreeMethod.
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
/* 创建一个新的链表. 创建的链表可能通过listRelease()释放, 
 * 但是每个节点的值需要, 用户在调用listRelease()之前释放掉, 
 * 或者通过listSetFreeMethod设置一个释放方法.
 * 
 * 创建失败会返回NULL, 成功则返回指向新链表的指针 */
list *listCreate(void)      /* 返回类型list  *   */
{
    struct list *list; /* 声明一个list结构体 */

    if ((list = zmalloc(sizeof(*list))) == NULL) /* 内存分配失败 */
        return NULL;

	/* 成员初始化 */
	list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list; /* 返回新链表的指针 */
}

/* Remove all the elements from the list without destroying the list itself. */
/* 在不影响链表本身的情况下，移除掉其中所有的元素 */
void listEmpty(list *list)
{
    unsigned long len;
    listNode *current, *next;

    current = list->head; /* 链表起始地址 */
    len = list->len; /* 链表长度 */

	/* 循环释放元素 */
	while(len--) {
        next = current->next;
        if (list->free) list->free(current->value);
        zfree(current);
        current = next;
    }

	/* 成员操作 */
    list->head = list->tail = NULL;
    list->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
/* 释放整个链表 */
void listRelease(list *list)
{
    listEmpty(list); /* 调用listEmpty()释放所有的元素 */
    zfree(list); /* 释放内存 */
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/* 向链表头部添加一个新节点
 * 错误则返回NULL 成功则返回传入的list指针 */
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL) /* 内存分配失败 */
        return NULL;
    node->value = value;
    if (list->len == 0) {                 /* 是一个空链表 */
        list->head = list->tail = node;   /* 添加的节点既是头节点也是尾节点 */
        node->prev = node->next = NULL;   /* 前继和后继节点指针为NULL */
    } else {                              /* 链表中已有节点的情况 */
        node->prev = NULL;
        node->next = list->head;          /* 将现有节点作为新添加节点的下一个节点 */
        list->head->prev = node;          /* 新节点与原头节点建立双向指向关系 */
        list->head = node;                /* 将添加的节点设置为新的头节点 */
    }
    list->len++;                          /* 链表长度加1 */
    return list;                          /* 返回链表 */
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/* 向链表尾部添加一个新节点.
 * 如果失败, 返回NULL, 原链表不受影响
 * 成功则返回传入的链表指针 */
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL) /* 内存分配失败 */
        return NULL;
    node->value = value;
    if (list->len == 0) {                      /* 是空链表     */
        list->head = list->tail = node;        /* 新节点既是头节点也是尾节点 */
        node->prev = node->next = NULL;        /* 新节点的前继和后继都为NULL */
    } else {                                   /* 非空链表 */
        node->prev = list->tail;               /* 将原尾节点作为新添加节点的前继 */
        node->next = NULL;                     /* 将新节点的后继设为NULL */
        list->tail->next = node;               /* 原尾节点的后继指向新添加节点 */
        list->tail = node;                     /* 将链表尾节点指向新添加节点 */
    }
    list->len++;                               /* 链表长度加1 */
    return list;
}

/* 向链表中插入节点
 * 在指定的原节点向前或向后插入新节点
 * 失败, 返回NULL 成功, 返回list */
list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    if ((node = zmalloc(sizeof(*node))) == NULL)      /* 内存分配失败 */
        return NULL;
    node->value = value;
    if (after) {                                      /* 在给定节点后插入 */
        node->prev = old_node;                        /* 新节点的前继为原节点 */
        node->next = old_node->next;                  /* 新节点的后继为原节点的后继 */
        if (list->tail == old_node) {                 /* 如果给定的节点为链表尾结点 */
            list->tail = node;                        /* 新添加节点此时为新的尾节点 */
        }
    } else {                                          /* 如果是在给定节点前插入 */
        node->next = old_node;                        /* 新添加节点的后继为给定节点 */
        node->prev = old_node->prev;                  /* 新节点的前继为给定节点的前继 */
        if (list->head == old_node) {                 /* 如果给定节点为头节点 */
            list->head = node;                        /* 则新节点为此时的头节点 */
        }
    }
    if (node->prev != NULL) {                         /* 将前一个节点的后继设为新节点 */
        node->prev->next = node;
    }
    if (node->next != NULL) {                         /* 将后一个节点的前继设为新节点 */
        node->next->prev = node;
    }
    list->len++;                                      /* 链表长度加1 */
    return list;
}


/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
/* 删除指定链表中的指定节点 */
void listDelNode(list *list, listNode *node)
{
    if (node->prev)
        node->prev->next = node->next;       /* 如果有前继指针,  则将前继指针的指向再后移一个节点 */
    else
        list->head = node->next;             /* 如果没有前继指针,即为头节点, 则将被删除节点的下一个节点作为头节点 */
    if (node->next)
        node->next->prev = node->prev;       /* 如果有后继指针,  则将后一个节点的前继指向被删除节点的前继 */
    else
        list->tail = node->prev;             /* 如果没有后继, 则链表的尾节点即为被删除节点的前一个节点 */
    if (list->free) list->free(node->value);
    zfree(node);
    list->len--;                             /* 链表长度减一 */
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
/* 返回链表迭代器
 * direction 控制迭代的方向 */
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;

    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    if (direction == AL_START_HEAD)
        iter->next = list->head;
    else
        iter->next = list->tail;
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
void listReleaseIterator(listIter *iter) {  /* 释放迭代器内存 */
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}

void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage
 * pattern is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
/* 返回迭代器指向的下一个节点 */
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

    if (current != NULL) {
        if (iter->direction == AL_START_HEAD)  /* 判断迭代的方向 */
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
/* 复制整个链表
 * 内存溢出时, 返回NULL */
list *listDup(list *orig)
{
    list *copy;
    listIter iter;
    listNode *node;

    if ((copy = listCreate()) == NULL)
        return NULL;
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    listRewind(orig, &iter);
    while((node = listNext(&iter)) != NULL) {
        void *value;

        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                return NULL;
            }
        } else
            value = node->value;
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            return NULL;
        }
    }
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
/* 通过给定的key查找一个节点 */
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    listRewind(list, &iter);  /* 重置迭代器 */
    while((node = listNext(&iter)) != NULL) {       /* 指向的下一个节点存在 */
        if (list->match) {                          /* 如果匹配到 */
            if (list->match(node->value, key)) {
                return node;                        /* 返回匹配到的节点 */
            }
        } else {
            if (key == node->value) {
                return node;
            }
        }
    }
    return NULL;                                    /* 没有匹配项, 返回NULL        */
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
/* 根据索引, 返回节点
 * 索引以0开始, 依此类推
 * 负数表示从尾部开始 */
listNode *listIndex(list *list, long index) {
    listNode *n;

    if (index < 0) {
        index = (-index)-1;
        n = list->tail;
        while(index-- && n) n = n->prev;  // 没看懂啊!!!
    } else {
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;
}



/* Rotate the list removing the tail node and inserting it to the head. */
/* 旋转链表, 将尾部节点插入到头节点 */
void listRotateTailToHead(list *list) {
    if (listLength(list) <= 1) return;

    /* Detach current tail */
    listNode *tail = list->tail; /* 拿到尾节点 */
    list->tail = tail->prev;     /* 将尾节点的前一个节点作为现在的尾节点 */
    list->tail->next = NULL;     /* 并将此时的尾节点后继指针置为NULL */
    /* Move it as head */        /* 开始移动操作 */
    list->head->prev = tail;     /* 将此时头节点的前继指针指向原尾节点 */
    tail->prev = NULL;           /* 将原尾节点的前继指针置为NULL */
    tail->next = list->head;     /* 将原尾节点的后继指针指向此时的头节点 */
    list->head = tail;           /* 原尾节点此时已成为新的头节点 */
}

/* Rotate the list removing the head node and inserting it to the tail. */
/* 旋转链表, 将头节点插入到尾节点中 */
void listRotateHeadToTail(list *list) {
    if (listLength(list) <= 1) return;

    listNode *head = list->head;
    /* Detach current head */
    list->head = head->next;
    list->head->prev = NULL;
    /* Move it as tail */
    list->tail->next = head;
    head->next = NULL;
    head->prev = list->tail;
    list->tail = head;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
void listJoin(list *l, list *o) {
    if (o->len == 0) return;

    o->head->prev = l->tail;

    if (l->tail)
        l->tail->next = o->head;
    else
        l->head = o->head;

    l->tail = o->tail;
    l->len += o->len;

    /* Setup other as an empty list. */
    o->head = o->tail = NULL;
    o->len = 0;
}
