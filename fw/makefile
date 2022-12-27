NAME    = GPS_Logger
DEVICE	= MSP430F5510
GCC_DIR	= c:/tools/gcc_msp430
INC_DIR	= $(GCC_DIR)/include
DIRS	= src src/lis3dh src/usb_config src/usb_app src/usb_app/fatfs \
	src/usb_app/fatfs/option src/usb_api/usb_common src/usb_api/usb_msc_api \
	src/driverlib/MSP430F5xx_6xx
SRCS	= $(foreach dir,$(DIRS),$(wildcard $(dir)/*.c))
OBJS	= $(patsubst %.c,obj/%.o,$(SRCS))
DIRO	= $(addprefix obj\,$(subst /,\,$(DIRS)))
CC	= $(GCC_DIR)/bin/msp430-elf-
CF	= $(addprefix -I ,$(INC_DIR) $(DIRS)) -D__$(DEVICE)__ -DDEPRECATED \
	-mmcu=$(DEVICE) -Os -fdata-sections -ffunction-sections -w
LF	= -L $(INC_DIR) -T src/usb_api/msp430USB.ld -T $(INC_DIR)/$(DEVICE).ld \
	-Wl,--gc-sections -Wl,-Map,obj/$(NAME).map

.PHONY:	all iar gcc clean res backup edit

all:	gcc iar

iar:
	c:\tools\iar\msp\common\bin\iarbuild.exe prj/$(NAME).ewp -make Debug
	$(CC)size $(NAME)(iar).hex

gcc:	$(DIRO) $(NAME)(GCC).hex $(NAME)(GCC).txt
	$(CC)size $(NAME)(GCC).hex

$(NAME)(GCC).txt: $(NAME)(GCC).hex
	srec_cat -O $@ -TITXT $< -I

$(NAME)(GCC).hex: obj/$(NAME).elf
	$(CC)objcopy -O ihex $< $@

obj/$(NAME).elf: $(OBJS)
	$(CC)gcc $(CF) $(LF) $^ -o $@

obj/%.o: %.c
	echo  $(notdir $<)
	$(CC)gcc $(CF) -Wa,-adhln -c $< -o $@ > $(basename $@).lst
$(DIRO):
	mkdir $@
clean:
	echo Clean
	rm -fr obj
run:
	echo Run $(NAME)($(TARGET))
	MSP430Flasher.exe -q -g -j fast -v -w $(NAME)($(TARGET)).hex -z [VCC]
res:
	echo Reset
	MSP430Flasher.exe -q -g -j fast -z [RESET, VCC]
backup:
	7z a -ssw -mx7 .\bup\$(shell date.exe +%y_%m_%d) prj src makefile readme *.*
edit:
	prj\edit -i %CD%\prj \
	$(wildcard src/usb_config/*.h) $(wildcard src/usb_app/fatfs/*.h) $(wildcard src/*.h) \
	$(wildcard src/usb_config/*.c) $(wildcard src/usb_app/fatfs/*.c) \
	$(filter-out src/main.c,$(wildcard src/*.c)) src/main.c
