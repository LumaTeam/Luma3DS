#pragma once

#include <stddef.h>
#include <stdbool.h>

/// Intrusive node structure definition.
typedef struct IntrusiveNode {
    struct IntrusiveNode *prev; ///< Pointer to the previous node.
    struct IntrusiveNode *next; ///< Pointer to the next node.
} IntrusiveNode;

/// Intrusive list structure definition.
typedef union IntrusiveList {
    IntrusiveNode node;
    struct {
        IntrusiveNode *last;     ///< Pointer to the last element.
        IntrusiveNode *first;    ///< Pointer to the first element.
    };
} IntrusiveList;

/**
 * @brief Initializes the intrusive linked list.
 *
 * @param ll The intrusive list to initialize.
 */
static inline void IntrusiveList_Init(IntrusiveList *ll)
{
    ll->node.prev = &ll->node;
    ll->node.next = &ll->node;
}

/**
 * @brief Tests if a node is past the end of the list containing it (if any).
 *
 * @param nd The node to test.
 * @return bool true iff node is past the end of the list.
 */
static inline bool IntrusiveList_TestEnd(const IntrusiveList *ll, const IntrusiveNode *nd)
{
    return nd == &ll->node;
}

/**
 * @brief Inserts a node after another one list.
 *
 * @param pos The node to insert the new node after.
 * @param nd The new node.
 */
static inline void IntrusiveList_InsertAfter(IntrusiveNode *pos, IntrusiveNode *nd)
{
    // if pos is last & list is empty, ->next writes to first, etc.
    pos->next->prev = nd;
    nd->prev = pos;
    nd->next = pos->next;
    pos->next = nd;
}

/**
 * @brief Erases a node.
 *
 * @param nd The node to erase.
 */
static inline void IntrusiveList_Erase(const IntrusiveNode *nd)
{
    nd->prev->next = nd->next;
    nd->next->prev = nd->prev;
}

/**
 * @brief Makes a list storage from a buffer, using each element's intrusive node field at offset 0.
 *
 * @param ll The intrusive list to create.
 * @param buf The buffer to use.
 * @param elem_size The size of each element.
 * @param total_size The total size of the buffer.
 *
 * @pre @ref ll has not been initialized yet.
 * @pre Each element must contain a @ref IntrusiveNode instance at offset 0, which is then used.
 */
static inline void IntrusiveList_CreateFromBuffer(IntrusiveList *ll, void *buf, size_t elemSize, size_t totalSize)
{
    IntrusiveList_Init(ll);
    for (size_t pos = 0; pos < totalSize; pos += elemSize) {
        IntrusiveList_InsertAfter(ll->last, (IntrusiveNode *)((char *)buf + pos));
    }
}
