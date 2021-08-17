# `A0-interrupt-dt`

> If you are not interested in how devicetree works, simply apply the devicetree overlay and skip the rest of all.

[TOC]

## What is it?

It's a piece of devicetree overlay decribing a device generating interrupt by rising edge on `GPIO17` of a Raspberry Pi 4B. 

## How to use it?

After `make`:

```
$ make
```

there will be a `.dtbo` file (a devicetree blob compiled from `.dts` file) to be overlayed:

```
$ ls
Makefile  arduino-irq.dtbo  arduino-irq.dts
```

To apply this devicetree overlay, first copy it to `/boot/firmware/overlays/` directory:

```
$ sudo cp arduino-irq.dtbo /boot/firmware/overlays/
```

then append `dtoverlay=arduino-irq` to  `sudo vim /boot/firmware/usercfg.txt` file:

```
# Place "config.txt" changes (dtparam, dtoverlay, disable_overscan, etc.) in
# this file. Please refer to the README file for a description of the various
# configuration files on the boot partition.
...
dtoverlay=arduino-irq
```

Finally reboot Raspberry Pi:

```
$ sudo reboot
```

After reboot, look up devicetree by decompiling `/proc/device-tree/` directory into `dts` file:

```
$ dtc -I fs /proc/device-tree/ -O dts | less
```

If devicetree overlay is applied successfully, you should be able to see node described in corresponding `.dts` file show up in decompiled devicetree. i.e. the following node will appear:

```
        arduino {
                interrupts = <0x11 0x01>;
                interrupt-parent = <0x0f>;
                compatible = "arduino,uno-irq";
                status = "ok";
                phandle = <0xda>;
        };
```

The `phandle` may change with different system configuration, but rest of the properties in this node should be the same.

## How does it work?

In high level viewpoint, a hardware event can be wired to another hardware (typically an interrupt controller) capable of translating such event to processor-comprehensible interrupt signals, so that processor can be aware of the existence of this event.  Such routing can be described in devicetree beforehand, so that kernel has ideas on device behind an interrupt signal, and how to deal with corresponding software abstraction of such interrupt-generating device inside kernel. 

This can be done by specifying `interrupt-parent` and `interrupts` properties in devicetree node of an interrupt-generating hardware, where `interrupt-parent` property specifies interrupt controller responsible for relaying this event to processor; and `interrupts` property specifies what the hardware event is and what it is translated to, from this particular `interrupt-parent`'s perspective (or in fancier term, its "interrupt domain"). 

> How to describe those properties in devicetree (or *bindings*, in devicetree terminologies) is controller-specific. In the case of Raspberry Pi 4B, it can be found in [*Broadcom BCM2835 GPIO (and pinmux) controller*](https://www.kernel.org/doc/Documentation/devicetree/bindings/pinctrl/brcm,bcm2835-gpio.txt).

In this case, `interrupt-parent` property is a reference (or `phandle` in devicetree termonologies) to GPIO controller on which it resides, meaning that hardware events happen on this device should be relayed to processor by, well, the GPIO controller itself. This is possible because this particular GPIO controller is capable of being an interrupt controller as well. The `<17 1>` in `interrupts` property decribes "rising-edge" event of `GPIO17`, meaning that if `interrupt-parent` sees a rising-edge event of `GPIO17` get triggered, it must come from hardware node whose `interrupts` property contains corresponding interrupt specifier describing that event, which in this case is the `arduino` platform device.

## Trivia

In case that you are not familiar with Linux driver model, a `platform_device` structure will be instantiated according to this devicetree node, which can be matched with a registered `platform_driver` whose `of_match_table` contains an entry with `compatible` string of the same contents. See next example.

## Where to go next?

Look up [*How Dealing with Modern Interrupt Architectures can Affect Your Sanity*](https://youtu.be/YE8cRHVIM4E) on ELCE 2016 in which Marc Zyngier introduced abstraction of interrupt generating hardware and interrupt routing inside Linux kernel. 

For detail in devicetree, see [*Device Tree 101*](https://youtu.be/a9CZ1Uk3OYQ), [*Device Tree: hardware description for everybody !*](https://youtu.be/Nz6aBffv-Ek), and [*Device Tree overlays and U-Boot extension board management*](https://youtu.be/mxWiK2v-KZc) webinars by *Bootlin*. Also the [official devicetree documentation](https://github.com/devicetree-org/devicetree-specification/releases/tag/v0.3) maintained by devicetree.org.
