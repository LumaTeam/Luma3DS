#pragma once

#include <3ds/types.h>
#include "MyThread.h"

MyThread *  PluginLoader__CreateThread(void);
bool        PluginLoader__IsEnabled(void);
void        PluginLoader__MenuCallback(void);
void        PluginLoader__UpdateMenu(void);
