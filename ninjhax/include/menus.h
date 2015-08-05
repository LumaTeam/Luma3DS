#pragma once

#include "textmenu.h"

#define BRAHMADIR "/brahma/"

s32 print_menu (s32 idx, struct menu_t *menu);
s32 print_file_list (s32 idx, struct menu_t *menu);
s32 print_main_menu (s32 idx, struct menu_t *menu);

s32 get_filename (s32 idx, char *buf, u32 size);

s32 menu_cb_load (s32 idx, void *param);
s32 menu_cb_choose_file (s32 idx, void *param);
s32 menu_cb_run (s32 idx, void *param);
s32 menu_cb_recv (s32 idx, void *param);
s32 menu_cb_patch_svc (s32 idx, void *param);

static const struct menu_t g_main_menu = {
	3,
	{
		{"Load ARM9 payload", &menu_cb_choose_file},
		{"Receive ARM9 payload", &menu_cb_recv},
		{"Run ARM9 payload", &menu_cb_run}
	}
};

static const struct menu_t g_file_list = {
	10,
	{
		{"Slot 0", &menu_cb_load},
		{"Slot 1", &menu_cb_load},
		{"Slot 2", &menu_cb_load},
		{"Slot 3", &menu_cb_load},
		{"Slot 4", &menu_cb_load},
		{"Slot 5", &menu_cb_load},
		{"Slot 6", &menu_cb_load},
		{"Slot 7", &menu_cb_load},
		{"Slot 8", &menu_cb_load},
		{"Slot 9", &menu_cb_load}
	}
};
