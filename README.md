# Linux IRQ Modules on Raspberry Pi

[TOC]

## What is it?

These are Linux kernel modules demonstrating different IRQ mechanisms in Linux kernel 5.4. Each of them can be triggered by rising edge on `GPIO17` of a Raspberry 4B. In particuler:

* Examples for dt-binding and basic `[devm_]request_irq()` function usage:

	1. `A0-interrupt-dt`: devicetree binding for an  interrupt generating device.
	2. `A1-request-irq`: getting IRQ number from devicetree and registering a handler.

* Examples of using generic work-deferring mechanisms as bottom-half of an IRQ:

	1. `B0-workqueue`: example of `workqueue` as bottom-half of an IRQ.
	2. `B1-tasklet`: example of `tasklet` as bottom-half of an IRQ.

* Example of threaded IRQ:

	1. `C0-threaded-irq`

See `README` in each directory for detail.

## Environment

These modules work on **Raspberry Pi 4B with 64-bit Ubuntu 20.04 Server**, which ships with **5.4** kernel. [Official installation guide](https://ubuntu.com/tutorials/how-to-install-ubuntu-on-your-raspberry-pi#1-overview) can be found on Ubuntu website. Later version of kernel should work fine, but tracing results my differ.

Also, everything should run on Raspberry Pi OS (formerly Raspbian) as well, except the way to apply devicetree overlay. See [*Using Device Trees on Raspberry Pi*](https://www.raspberrypi.org/documentation/computers/configuration.html#using-device-trees-on-raspberry-pi) section on Raspberry Pi OS documentation.

## How to set up?

### 0. Prerequisites

Install dependencies for building kernel modules:

```
$ sudo apt install make device-tree-compiler linux-headers-$(uname -r)
```

If you want to trace IRQ handlers with `trace-cms`, a ftrace command line front-end:

```
$ sudo apt install trace-cmd
```

If you want to trigger IRQ with `gpioset` instead of set up external hardware, or check gpio status with `gpioinfo`:

```
$ sudo apt install gpiod
```

If you want to hook tracepoints with `bpftrace`:

```
$ sudo apt install bpftrace
```

Note that `bpftrace` may be unavailable on some platforms.

### 1. Apply Devicetree Overlay

You'll have to apply devicetree overlay in `00-interrupt-dts` before installing any module;  otherwise the modules won't `probe()`. To do this, 

1. Go to `00-interrupt-dt` directory and `make`:

	```
	$ cd 00-interrupt-dt
	$ make
	```

	There will be a `arduino-irq.dtbo` file to be overlayed:

	```
	$ ls
	Makefile  arduino-irq.dtbo  arduino-irq.dts
	```

2. Copy `.dtbo` file to `/boot/firmware/overlays/` directory:

	```
	$ sudo cp arduino-irq.dtbo /boot/firmware/overlays/
	```

3. Append `dtoverlay=arduino-irq` to  `/boot/firmware/usercfg.txt` file:

	```
	# Place "config.txt" changes (dtparam, dtoverlay, disable_overscan, etc.) in
	# this file. Please refer to the README file for a description of the various
	# configuration files on the boot partition.
	dtoverlay=arduino-irq
	```

	Modify the file with text editor you prefer, say, `vim`:
	
	```
	$ sudo vim /boot/firmware/usercfg.txt
	```
    
    or redirect from `echo`.

4. finally reboot:

	```
	$ sudo reboot
	```

5. After reboot, verify by looking up devicetree decompiled from `/proc/device-tree/` directory with `dtc`:

	```
	$ dtc -I fs /proc/device-tree/ -O dts | less
	```

	If devicetree overlay is applied successfully, node described in `arduino-irq.dts` will show up in decompiled devicetree:

	```
        arduino {
                interrupts = <0x11 0x01>;
                interrupt-parent = <0x0f>;
                compatible = "arduino,uno-irq";
                status = "ok";
                phandle = <0xda>;
        };
	```

	> You can search text by `/` command in `less`

	The `phandle` property may change under different system configuration, but other properties should be the same.

### 2. Build and Install Module

1. To build module in each directory, simply enter and `make`:

	```
	$ make
	```
	
	after `make`, there will be a `arduino-irq.ko`, which is the kernel module to be installed.

2. Kernel modules can be installed by `insmod`:

	```
	$ sudo insmod arduino-irq.ko
	```

	If one of these modules is installed successfully, the following message will show up in `dmesg`

	```
	[  441.476796] arduino_irq arduino: successfully probed arduino!
	```

3. To remove a module, simply use `rmmod` command:

	```
	$ sudo rmmod arduino_irq
	```

## Trigger the IRQ

You can either trigger this IRQ with `gpioset` command, or by setting up external hardware.

### By `gpioset`

To trigger itthis IRQ with `gpioset`, simply use:

```
$ sudo gpioset gpiochip0 17=1
```

`gpioset` will set GPIO line values of a GPIO chip and maintain the state until the process exits. See `man gpioset` for detail.

### By External Hardware

Alternatively, you can trigger this IRQ with external hardware. Any hardware capable of toggling GPIO can be used. Here is an  Arduino Uno example:

1. Connect `A0` on Arduino Uno through proper interface (e.g. a logic level shifter) to `GPIO17` on Raspberry Pi 4B:

	![](https://i.imgur.com/x59hgtI.jpg)

	![](https://i.imgur.com/tqDiw5R.png)

3. Connect Arduino Uno to personal computer and upload `trigger.ino`to Arduino. 

4. You can now fire IRQ manually by sending pressing return to Serial Monitor in Arduino IDE:

	![](https://i.imgur.com/3iwz6hG.png)


Other hardware setup, like a push bottom, is possible. Do note that these modules don't handle debouncing.


## Dynamic Tracing with `trace-cmd`

`trace-cmd` is a command line front-end of `ftrace`, the official tracer of Linux kernel. You can look into internals of IRQ in Linux kernel with different tracers in ftrace.

### TL;DR

After installing one of these modules, do the following:

1. Run one of the following `trace-cmd` commands: 

	* To graph the flow of execution of top-half function:

		```
		$ sudo trace-cmd record \
			-p function_graph \
			--max-graph-depth 5 \
			-g arduino_irq_top_half \
			&
		```
	
	* To record the flow of execution of both top-half and bottom-half function:

		```
		$ sudo trace-cmd record \
			-p function_graph \
			--max-graph-depth 5 \
			-g arduino_irq_top_half \
			-g arduino_irq_bottom_half \
			&
		```
        
        Note that it only works when a module with bottom-half handler installed.


	* Record top-half function stack:
	
		```
		$ sudo trace-cmd record \
			-p function \
			--func-stack \
			-l arduino_irq_top_half \
			&
		```

	* Record both top-half and bottom-half function (if there is one) stack:

		```
		$ sudo trace-cmd record \
			-p function \
			--func-stack \
			-l arduino_irq_top_half \
			-l arduino_irq_bottom_half \
			&
		```	

3. (Optional) add some work loads. Say `ping` if your RPi is conntected to the internet:

	```
	$ sudo ping -i 0.001 ubuntu &> /dev/null &
	```

	where `-i` option sepcifies the interval between each ping. This should be adjusted according to quiality of network connection.

3. Poke it with Arduino, orwith `gpioset` for a few times:
	
	```
	$ sudo gpioset gpiochip0 17=1
	```

4. (Optional) `fg` and then Ctrl + C to terminate `ping`.

	```
	$ fg
	^C
	```

5. `fg` and then Ctrl + C to terminate `trace-cmd`.

6. Use `-l` option for `trace-cmd` to see report:

	```
	$ trace-cmd report -l | less
	```

### Tracing Context of Execution

To graph internals of a function in kernel, simply set `function_graph` tracer with `-g` option, which specifies functions of interest. 

In this case,  `trace-cmd` is set to trace `arduino_irq_top_half()` function (by `-g`) in background (by `&`). The tracer (`function_graph`) is set by `-p` option, with proper `--max-graph-depth`:

```
$ sudo trace-cmd record \
	-p function_graph \
	--max-graph-depth 5 \
	-g arduino_irq_top_half \
	&
```
For those modules whose bottom-half function is implemented, you can graph both of them at the same time. Just add another `-g` option, say:

```
$ sudo trace-cmd record \
	-p function_graph \
	--max-graph-depth 5 \
	-g arduino_irq_top_half \
	-g arduino_irq_bottom_half \
	&
```

After setting up `trace-cmd`, poke the IRQ for a few times with Arduino or `gpioset`:

```
$ sudo gpioset gpiochip0 17=1
```

When done, move `trace-cmd` back to foreground by `fg`:

```
$ fg
```

and then `report` with `-l` option:

```
$ trace-cmd report -l | less
```

If succeeded, you'll see traceing report like this:

```c
    ping-2653    0d.h.   870.097107: funcgraph_entry:                   |  arduino_irq_top_half() {
    ping-2653    0d.h.   870.097111: funcgraph_entry:                   |    _dev_info() {
    ping-2653    0d.h.   870.097112: funcgraph_entry:                   |      __dev_printk() {
    ping-2653    0d.h.   870.097113: funcgraph_entry:                   |        dev_printk_emit() {
    ping-2653    0d.h.   870.097114: funcgraph_entry:      + 17.315 us  |          dev_vprintk_emit();
    ping-2653    0d.h.   870.097133: funcgraph_exit:       + 19.370 us  |        }
    ping-2653    0d.h.   870.097133: funcgraph_exit:       + 21.203 us  |      }
    ping-2653    0d.h.   870.097134: funcgraph_exit:       + 22.870 us  |    }
    ping-2653    0d.h.   870.097135: funcgraph_exit:       + 28.556 us  |  }
irq/43-a-2624    1....   870.097159: funcgraph_entry:                   |  arduino_irq_bottom_half() {
irq/43-a-2624    1....   870.097162: funcgraph_entry:                   |    _dev_info() {
irq/43-a-2624    1....   870.097163: funcgraph_entry:                   |      __dev_printk() {
irq/43-a-2624    1....   870.097164: funcgraph_entry:                   |        dev_printk_emit() {
irq/43-a-2624    1....   870.097166: funcgraph_entry:      + 12.648 us  |          dev_vprintk_emit();
irq/43-a-2624    1....   870.097179: funcgraph_exit:       + 14.963 us  |        }
irq/43-a-2624    1....   870.097180: funcgraph_exit:       + 17.334 us  |      }
irq/43-a-2624    1....   870.097181: funcgraph_exit:       + 19.574 us  |    }
irq/43-a-2624    1....   870.097182: funcgraph_exit:       + 23.314 us  |  }
```

This is an excert of tracing result from the `C0-threaded-irq
`. Latency format for these functions reveal context of execution. For example:

1. `arduino_irq_top_half()` (`0d.h.`): CPU0 executed it in hard IRQ context (`h`), with local irq disabled (`d`).
2. `arduino_irq_bottom_half()` (`1....`): CPU1 ran it in process context (second `.` from the right).

Different module uses different IRQ mechanism, so the result depends on the module being installed. Detailed analysis for each case can be found in README of each module.

### Tracing Function Stack

Alternatively, you can trace function stack with `function` tracer. Simply replace the `trace-cmd` command from `function_graph` tracer to `function` tracer and add `--function_stack` option, e.g.:

```
$ sudo trace-cmd record \
	-p function \
	--func-stack \
	-l arduino_irq_top_half \
	&
```

or if any bottom-half function is implemented:

```
$ sudo trace-cmd record \
	-p function \
	--func-stack \
	-l arduino_irq_top_half \
	-l arduino_irq_bottom_half \
	&
```

After setting up `trace-cmd`, poke the IRQ for a few time with Arduino or `gpioset`:

```
$ sudo gpioset gpiochip0 17=1
```

When done, stop `trace-cmd` with Ctrl + C after moving `trace-cmd` back to foreground by `fg`:

```
$ fg
```

and then `report` with `-l` option:

```
$ trace-cmd report -l | less
```

If succeeded, you'll see tracing report with repeating format like this:

```diff
    ping-4211    0..s2  8960.501664: function:             arduino_irq_bottom_half
    ping-4211    0..s2  8960.501665: kernel_stack:         <stack trace>
=> ftrace_graph_call (ffffa31c3f09f7a8)
=> arduino_irq_bottom_half (ffffa31c02ff6240)
=> tasklet_action_common.isra.0 (ffffa31c3f0f8230)
=> tasklet_action (ffffa31c3f0f8348)
=> __do_softirq (ffffa31c3f081cd8)
=> irq_exit (ffffa31c3f0f7ffc)
=> __handle_domain_irq (ffffa31c3f16a2ac)
=> gic_handle_irq (ffffa31c3f08181c)
+=> el1_irq (ffffa31c3f083788)
=> _raw_spin_unlock_irqrestore (ffffa31c3fbbf948)
=> __skb_try_recv_datagram (ffffa31c3f9d7338)
=> __skb_recv_datagram (ffffa31c3f9d746c)
=> skb_recv_datagram (ffffa31c3f9d7510)
=> ping_recvmsg (ffffa31c3fad8624)
=> inet_recvmsg (ffffa31c3fac4d50)
=> sock_recvmsg (ffffa31c3f9bda54)
=> ____sys_recvmsg (ffffa31c3f9bec04)
=> ___sys_recvmsg (ffffa31c3f9beec8)
=> __sys_recvmsg (ffffa31c3f9c3d68)
=> __arm64_sys_recvmsg (ffffa31c3f9c3df0)
=> el0_svc_common.constprop.0 (ffffa31c3f09bd7c)
=> el0_svc_handler (ffffa31c3f09bf48)
+=> el0_svc (ffffa31c3f084690)
```

This is an excert of report genereted from `B1-tasklet` example, showing an IRQ interupting `ping` inside a system call it invoked, and `arduino_irq_bottom_half()` was going to run inside a  tasklet, a category soft IRQ mechanisms, when processor exiting a hard IRQ. See detail in `B1-tasklet` module.
