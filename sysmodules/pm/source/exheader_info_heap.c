#include <3ds.h>
#include <string.h>
#include <stdatomic.h>

typedef union Node {
    union Node *next;
    ExHeader_Info info;
} Node;

static Node *g_headNode = NULL;

void ExHeaderInfoHeap_Init(void *buf, size_t num)
{
    Node *node = (Node *)buf;
    g_headNode = node;
    for (size_t i = 0; i < num; i++) {
        node->next = &node[1];
        node = node->next;
    }
}

ExHeader_Info *ExHeaderInfoHeap_New(void)
{
    Node *newHead;
    Node *oldHead = atomic_load(&g_headNode);
    if (oldHead == NULL) {
        return NULL;
    } else {
        do {
            newHead = oldHead == NULL ? oldHead : oldHead->next;
        } while(!atomic_compare_exchange_weak(&g_headNode, &oldHead, newHead));

        if (oldHead == NULL) {
            return NULL;
        }
        else {
            memset(&oldHead->info, 0, sizeof(ExHeader_Info));
            return &oldHead->info;
        }
    }
}

void ExHeaderInfoHeap_Delete(ExHeader_Info *data)
{
    Node *newHead = (Node *)data;
    Node *oldHead = atomic_load(&g_headNode);
    do {
        newHead->next = oldHead;
    } while(!atomic_compare_exchange_weak(&g_headNode, &oldHead, newHead));
}

