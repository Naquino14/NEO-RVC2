CWD := $(shell pwd)

BAUDRATE := 115200

# Note this is not for when multiple boards are connected
ifneq ("$(wildcard /dev/ttyACM0)","")
BOARD_DEV := /dev/ttyACM0

else ifneq ("$(wildcard /dev/ttyUSB0)","")
BOARD_DEV := /dev/ttyUSB0

else
$(error "No board found at /dev/ttyACM0 aka TRC, or /dev/ttyUSB0 aka FOB")
BOARD_DEV := none
endif

auto:
	@if [ "$(BOARD_DEV)" = "/dev/ttyACM0" ]; then \
		$(MAKE) trc; \
	elif [ "$(BOARD_DEV)" = "/dev/ttyUSB0" ]; then \
		$(MAKE) fob; \
	else \
		exit 1; \
	fi

fob:
	west build -b heltec_wifi_lora32_v3/esp32s3/procpu --sysbuild -s app -p auto -- \
		-DCONFIG_DEVICE_ROLE=1 -DBOARD_ROOT=$(CWD) -DDTC_OVERLAY_FILE=$(CWD)/app/boards/heltec_wifi_lora32_v3_procpu.overlay

trc:
	west build -b heltec_htit_tracker/esp32s3/procpu --sysbuild -s app -p auto -- \
	-DCONFIG_DEVICE_ROLE=2 -DBOARD_ROOT=$(CWD) -DDTC_OVERLAY_FILE=$(CWD)/app/boards/heltec_htit_tracker_procpu.overlay

flash-fob:
	west flash --esp-device /dev/ttyUSB0

flash-trc:
	west flash --esp-device /dev/ttyACM0

flash: 	# automagic flash
	west flash --esp-device $(BOARD_DEV)

mon-fob:
	minicom -D /dev/ttyUSB0 -b $(BAUDRATE)

mon-trc:
	minicom -D /dev/ttyACM0 -b $(BAUDRATE)

mon: 	# automagic monitor
	minicom -D $(BOARD_DEV) -b $(BAUDRATE)

menuconfig:
	west build -t menuconfig

clean:
	rm -rf build/

show:
	@echo CWD: $(CWD)
