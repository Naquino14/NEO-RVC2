CWD := $(shell pwd)

fob:
	west build -b heltec_wifi_lora32_v3/esp32s3/procpu --sysbuild -s app -p auto -- \
		-DCONFIG_DEVICE_ROLE=1 -DBOARD_ROOT=$(CWD) -DDTC_OVERLAY_FILE=$(CWD)/app/boards/heltec_wifi_lora32_v3_procpu.overlay

trc:
	west build -b heltec_htit_tracker/esp32s3/procpu --sysbuild -s app -p auto -- \
	-DCONFIG_DEVICE_ROLE=2 -DBOARD_ROOT=$(CWD) -DDTC_OVERLAY_FILE=$(CWD)/app/boards/heltec_htit_tracker_procpu.overlay

flash:
	west flash

mon-fob:
	minicom -D /dev/ttyUSB0 -b 115200

mon-trc:
	minicom -D /dev/ttyACM0 -b 115200

mon:
	@if [ -e /dev/ttyUSB0 ]; then \
		echo "Using /dev/ttyUSB0"; \
		minicom -D /dev/ttyUSB0 -b 115200; \
	elif [ -e /dev/ttyACM0 ]; then \
		echo "Using /dev/ttyACM0"; \
		minicom -D /dev/ttyACM0 -b 115200; \
	else \
		echo "No serial device found."; \
	fi

menuconfig:
	west build -t menuconfig

clean:
	rm -rf build/

show:
	@echo CWD: $(CWD)
