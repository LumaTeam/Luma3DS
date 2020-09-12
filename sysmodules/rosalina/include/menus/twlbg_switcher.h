#include <3ds.h>

#define MAX_FILES 18
#define FILE_NAME_MAX 256

void TwlbgSwitcher_DisplayFiles(void);
Result TwlbgSwitcher_PopulateFiles(void);
Result TwlbgSwitcher_WriteNameToFile(char filename[FILE_NAME_MAX]);
Result TwlbgSwitcher_ReadNameFromFile(void);
Result TwlbgSwitcher_SwitchTwlbg(char filename[FILE_NAME_MAX]);
void TwlbgSwitcher_UpdateMenu(void);
void TwlbgSwitcher_UpdateStatus(void);
