#pragma once

typedef int menu_func_t (s32, void *);

typedef struct menu_elem_t {
	const char *name;
	menu_func_t *func;
} _menu_elem_t; 

typedef struct menu_t {
	s32 element_count;
	struct menu_elem_t element[];
} _menu_t;

s32 menu_get_element_count (struct menu_t *menu);
s32 menu_is_valid_index (s32 idx, struct menu_t *menu);
s32 menu_update_index (s32 idx, struct menu_t *menu);
const char *menu_get_element_name (s32 idx, struct menu_t *menu);
menu_func_t *menu_get_element_function (s32 idx, struct menu_t *menu);
s32 menu_execute_function (s32 idx, struct menu_t *menu, void *param);
