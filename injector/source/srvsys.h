/**
 * @file srv.h
 * @brief Service API.
 */
#pragma once

/// Initializes the service API.
Result srvSysInit(void);

/// Exits the service API.
Result srvSysExit(void);

/**
 * @brief Retrieves a service handle, retrieving from the environment handle list if possible.
 * @param out Pointer to write the handle to.
 * @param name Name of the service.
 */
Result srvSysGetServiceHandle(Handle* out, const char* name);

/// Registers the current process as a client to the service API.
Result srvSysRegisterClient(void);

/**
 * @brief Enables service notificatios, returning a notification semaphore.
 * @param semaphoreOut Pointer to output the notification semaphore to.
 */
Result srvSysEnableNotification(Handle* semaphoreOut);

/**
 * @brief Receives a notification.
 * @param notificationIdOut Pointer to output the ID of the received notification to.
 */
Result srvSysReceiveNotification(u32* notificationIdOut);

/**
 * @brief Registers the current process as a service.
 * @param out Pointer to write the service handle to.
 * @param name Name of the service.
 * @param maxSessions Maximum number of sessions the service can handle.
 */
Result srvSysRegisterService(Handle* out, const char* name, int maxSessions);

/**
 * @brief Unregisters the current process as a service.
 * @param name Name of the service.
 */
Result srvSysUnregisterService(const char* name);
