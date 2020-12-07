/*
list.c

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#include "list.h"
#include "common.h"

struct ListBase;
typedef struct ListNodeBase
{
    struct ListNodeBase *prev, *next;
    struct ListBase *parent;
} ListNodeBase;

typedef struct ListBase
{
    ListNodeBase *first, *last;
} ListBase;

void buildList(void *list, void *pool, u32 nb, u32 elementSize)
{
    ListBase *listB = (ListBase *)list;
    for(u32 i = 0; i < nb; i++)
    {
        ListNodeBase *node = (ListNodeBase *)((u8 *)pool + i * elementSize);
        node->prev = i == 0 ? NULL : (ListNodeBase *)((u8 *)pool + (i - 1) * elementSize);
        node->next = i == (nb - 1) ? NULL : (ListNodeBase *)((u8 *)pool + (i + 1) * elementSize);
        node->parent = list;
    }

    listB->first = (ListNodeBase *)pool;
    listB->last = (ListNodeBase *)((u8 *)pool + (nb - 1) * elementSize);
}

void moveNode(void *node, void *dst, bool back)
{
    ListNodeBase *nodeB = (ListNodeBase *)node;
    ListBase *dstB = (ListBase *)dst;
    ListBase *srcB = nodeB->parent;

    if(dstB == srcB)
        return;

    // Remove the node in the source list
    if(nodeB->prev != NULL)
        nodeB->prev->next = nodeB->next;

    if(nodeB->next != NULL)
        nodeB->next->prev = nodeB->prev;

    // Update the source list if needed
    if(nodeB == srcB->first)
        srcB->first = nodeB->next;
    if(nodeB == srcB->last)
        srcB->last = nodeB->prev;

    // Insert the node in the destination list
    if(back)
    {
        if(dstB->last != NULL)
            dstB->last->next = nodeB;

        nodeB->prev = dstB->last;
        nodeB->next = NULL;
        dstB->last = nodeB;
    }
    else
    {
        if(dstB->first != NULL)
            dstB->first->prev = nodeB;

        nodeB->next = dstB->first;
        nodeB->prev = NULL;
        dstB->first = nodeB;
    }

    // Normalize the destination list
    if(dstB->first != NULL && dstB->last == NULL)
        dstB->last = dstB->first;
    else if(dstB->first == NULL && dstB->last != NULL)
        dstB->first = dstB->last;

    nodeB->parent = dstB;
}

void *allocateNode(void *inUseList, void *freeList, u32 elementSize, bool back)
{
    ListBase *freeListB = (ListBase *)freeList;

    if(freeListB->first == NULL)
        panic(0);

    ListNodeBase *node = freeListB->first;
    ListNodeBase nodeBk = *node;

    memset(node, 0, elementSize);
    *node = nodeBk;

    moveNode(node, inUseList, back);

    return node;
}
