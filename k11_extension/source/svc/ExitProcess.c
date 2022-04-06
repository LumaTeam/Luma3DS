#include "svc/ExitProcess.h"

void ExitProcessHook(void) {
    KProcess *currentProcess = currentCoreContext->objectContext.currentProcess;
    u32      flags = KPROCESS_GET_RVALUE(currentProcess, customFlags);

    if (flags & SignalOnExit)
    {
        // Signal that the process is about to be terminated
        if (PLG_GetStatus() == PLG_CFG_RUNNING)
            PLG_SignalEvent(PLG_CFG_EXIT_EVENT);

        // Unlock all threads that might be locked
        {
            KRecursiveLock__Lock(criticalSectionLock);

            for (KLinkedListNode *node = threadList->list.nodes.first;
                node != (KLinkedListNode *)&threadList->list.nodes;
                node = node->next)
            {
                KThread *thread = (KThread *)node->key;

                if (thread->ownerProcess == currentProcess && thread->schedulingMask & 0x20)
                    thread->schedulingMask &= ~0x20;
            }

            KRecursiveLock__Unlock(criticalSectionLock);
        }
    }

    return ((void(*)())officialSVCs[0x3])();
}