MODULE_NAME = arduino-irq

all:
	dtc -@ -I dts -O dtb -o $(MODULE_NAME).dtbo $(MODULE_NAME).dts
clean:
	rm $(MODULE_NAME).dtbo
