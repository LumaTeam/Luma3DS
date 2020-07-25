/*
notifications.h

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include "common.h"

Result EnableNotification(SessionData *sessionData, Handle *notificationSemaphore);
Result Subscribe(SessionData *sessionData, u32 notificationId);
Result Unsubscribe(SessionData *sessionData, u32 notificationId);
Result ReceiveNotification(SessionData *sessionData, u32 *notificationId);
Result PublishToSubscriber(u32 notificationId, u32 flags);
Result PublishAndGetSubscriber(u32 *pidCount, u32 *pidList, u32 notificationId, u32 flags);
Result PublishToProcess(Handle process, u32 notificationId);
Result PublishToAll(u32 notificationId);

Result AddToNdmuWorkaroundCount(s32 count);
