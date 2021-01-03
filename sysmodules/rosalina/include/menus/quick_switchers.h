#include <3ds.h>

#define MAX_FILES 18
#define FILE_NAME_MAX 256
#define NO_OF_SWITCHABLES 3

extern Menu quickSwitchersMenu;

void QuickSwitchers_DisplayFiles(void);
Result QuickSwitchers_PopulateFiles(void);
Result QuickSwitchers_WriteNameToFile(char filename[FILE_NAME_MAX]);
Result QuickSwitchers_ReadNameFromFile(void);
Result QuickSwitchers_SwitchFile(char filename[FILE_NAME_MAX]);
void QuickSwitchers_UpdateMenu(void);
void QuickSwitchers_UpdateStatus(void);
void QuickSwitchers_UpdateStatuses(void);
void QuickSwitchers_TwlBg(void);
void QuickSwitchers_AgbBg(void);
void QuickSwitchers_OpenAgb(void);