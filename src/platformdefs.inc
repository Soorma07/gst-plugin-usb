ifeq ($(PLATFORM),dm3730)
 XDC_PLATFORM=ti.platforms.evm3530
 XDC_TARGET=gnu.targets.arm.GCArmv5T
else
 $(error Unsupported platform: $(PLATFORM))
endif

export XDC_PLATFORM
export XDC_TARGET
