.text
.arm
.align 4

.global svcGetCFWInfo
.type	svcGetCFWInfo, %function
svcGetCFWInfo:
	svc 0x2e
	bx lr
