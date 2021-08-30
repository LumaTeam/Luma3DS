typedef struct {
	bool suppressLeds;
	bool cutSlotPower;
	bool cutSleepWifi;
} config_extra;

extern config_extra configExtra;
extern Menu configExtraMenu;

void ConfigExtra_SetSuppressLeds();
void ConfigExtra_SetCutSlotPower();
void ConfigExtra_SetCutSleepWifi();
void ConfigExtra_UpdateMenuItem(int menuIndex, bool value);
void ConfigExtra_UpdateAllMenuItems();
void ConfigExtra_ReadConfigExtra();
void ConfigExtra_WriteConfigExtra();
