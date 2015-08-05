#include <dirent.h>
#include <3ds.h>
#include "menus.h"

s32 print_menu (s32 idx, struct menu_t *menu) {
	s32 i;
	s32 newidx;
	s32 count = menu_get_element_count(menu);
	
	newidx = menu_update_index(idx, menu);
	for (i=0; i<count; i++) {
		if (newidx == i)
			printf("[ %s ]\n", menu_get_element_name(i, menu));
		else
			printf("  %s  \n", menu_get_element_name(i, menu));
	}
	return newidx;	
}

s32 print_file_list (s32 idx, struct menu_t *menu) {	
	s32 i = 0;
	s32 newidx;
	DIR *dp;
	struct dirent *entry;
	char *filename = 0;
	s32 totalfiles = 0;
	s32 num_printed = 0;

	consoleClear();

	printf("ARM9 payload (%s):\n\n\n", BRAHMADIR);
	printf("===========================\n");

	s32 count = menu_get_element_count(menu);
	
	newidx = menu_update_index(idx, menu);
	if((dp = opendir(BRAHMADIR))) {
		for (i=0; i<count; i++) {
			if ((entry = readdir(dp)) != 0) {
				filename = entry->d_name;
			}
			else {
				filename = "---";
			}
			if (newidx == i)
				printf("[ %s ] %s\n", menu_get_element_name(i, menu), filename);
			else
				printf("  %s   %s\n", menu_get_element_name(i, menu), filename);
		}
		closedir(dp);
	}
	else {
		printf("[!] Could not open '%s'\n", BRAHMADIR);
	}

	printf("===========================\n\n");
	printf("A:     Confirm\n");
	printf("B:     Back\n");

	return newidx;	
}

s32 print_main_menu (s32 idx, struct menu_t *menu) {
	s32 newidx = 0;
	consoleClear();

	printf("\n* BRAHMA *\n\n\n");
	printf("===========================\n");
	newidx = print_menu(idx, menu);
	printf("===========================\n\n");	
	printf("A:     Confirm\n");
	printf("B:     Exit\n");

	return newidx;
}

s32 get_filename (s32 idx, char *buf, u32 size) {
	DIR *dp;
	struct dirent *entry;
	s32 result = 0;
	s32 numfiles = 0;

	if((dp = opendir(BRAHMADIR)) && buf && size) {   	
		while((entry = readdir(dp)) != NULL) {
			if (numfiles == idx) {
				snprintf(buf, size-1, "%s%s", BRAHMADIR, entry->d_name);
				result = 1;
				break;
			}
			numfiles++;
		}
		closedir(dp);
	}
	return result;
}

s32 menu_cb_recv (s32 idx, void *param) {
	return recv_arm9_payload();
}

s32 menu_cb_load(s32 idx, void *param) {
	char filename[256];
	s32 result = 0;

	if (param) {
		if (get_filename(*(u32 *)param, &filename, sizeof(filename))) {
			printf("[+] Loading %s\n", filename);
			result = load_arm9_payload(filename);
		}
	}
	return result;
}

s32 menu_cb_choose_file (s32 idx, void *param) {
	s32 curidx = idx;
	s32 loaded = 0;

	while (aptMainLoop()) {
		gspWaitForVBlank();

		curidx = print_file_list(curidx, &g_file_list);	           
		u32 kDown = wait_key(); 

		if (kDown & KEY_B) {
			break;
		}
		else if (kDown & KEY_A) {
			consoleClear();
			loaded = menu_execute_function(curidx, &g_file_list, &curidx);
			printf("%s\n", loaded? "[+] Success":"[!] Failure");
			wait_any_key();
			if (loaded)
				break;
		}
		else if (kDown & KEY_UP) {
			curidx--;
		}
		else if (kDown & KEY_DOWN) {
			curidx++;
		}
		gfxFlushBuffers();
		gfxSwapBuffers();
	}
	return 0;
}

s32 menu_cb_run (s32 idx, void *param) {
	s32 fail_stage;

	/* we're kinda screwed if the exploit fails
	   and soc has been deinitialized. not sure
	   whether cleaning up here improves existing
	   problems with using sockets either */
	soc_exit();
	printf("[+] Running ARM9 payload\n");	
	fail_stage = firm_reboot();

	char *msg;
	switch (fail_stage) {
		case 1:
			msg = "[!] ARM11 exploit failed";
			break;
		case 2:
			msg = "[!] ARM9 exploit failed";
			break;
		default:
			msg = "[!] Unexpected error";
	}
	printf("%s\n", msg);
	return 1;
}
