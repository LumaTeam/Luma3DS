#include <3ds.h>
#include "textmenu.h"

s32 menu_get_element_count (struct menu_t *menu) {
	s32 i = 0;

	if (menu) {
		i = menu->element_count;
	}
	return i;
}

s32 menu_is_valid_index (s32 idx, struct menu_t *menu) {
	return (menu != 0 && (idx >= 0 && idx < menu_get_element_count(menu)));
}

s32 menu_update_index (s32 idx, struct menu_t *menu) {
	s32 newidx = 0;
	s32 count = menu_get_element_count(menu);

	newidx = idx < 0 ? count - 1 : idx >= count ? 0 : idx;

	return newidx;
}

const char *menu_get_element_name (s32 idx, struct menu_t *menu) {	
	return menu_is_valid_index(idx, menu) ? menu->element[idx].name : 0;
}

menu_func_t *menu_get_element_function (s32 idx, struct menu_t *menu) {
	return menu_is_valid_index(idx, menu) ? menu->element[idx].func : 0;
}

s32 menu_execute_function (s32 idx, struct menu_t *menu, void *param) {
	s32 result = 0;
	menu_func_t *f;	

	if (menu_is_valid_index(idx, menu)) {
		f = menu_get_element_function(idx, menu);
		if (f)
			result = f(idx, param);
	}

	return result;
}
