#include <3ds.h>
#include "sochlp.h"

u32 soc_init (void) {
	Result ret;
	u32 result = 0;
	
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	if (SOC_buffer != 0) {
		ret = SOC_Initialize(SOC_buffer, SOC_BUFFERSIZE);
		if (ret == 0) {
			result = 1;
		} else {
			free(SOC_buffer);
		}
	}
	return result;
}

u32 soc_exit (void) {
	if (SOC_buffer) {
		SOC_Shutdown();
		free(SOC_buffer);
		SOC_buffer = 0;
	}
	return 0;
}
