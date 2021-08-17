# `B2-threaded-irq`

> Make sure you already applied devicetree overlay in `00-interrupt-dt` before you start.

[TOC]

## What is it?

It's a kernel module registering rising-edge event of `GPIO17` on Raspberry Pi 4B as a **threaded IRQ**.

## How to use it?

After `make` 

```
$ make
```

and installing the module:

```
$ sudo insmod arduino-irq.ko
```

trigger it with Arduino or `gpioset`:

```
$ sudo gpioset gpiochip0 17=1
```

Upon success, the following messages will show up in `dmesg`:

```
[ 1820.715887] arduino_irq arduino: irq=43 top-half executed
[ 1820.715916] arduino_irq arduino: irq=43 bottom-half executed
```

## How does it work?
 
After claiming the interrupt line inside `probe()` with `platform_get_irq()`, the `probe()` function then and registers a *threaded IRQ* by providing a top-half function `arduino_irq_top_half()`, whcih will be executed in hard IRQ context, and a bottom-half function `arduino_irq_bottom_half()`, which will be executed by dedicated IRQ thread in process context, to  `devm_request_threaded_irq()`, the "managed" version of `request_threaded_irq()`. 

When an IRQ is registered by the  `request_threaded_irq()` functions, dedicated thread will be created to run registered bottom-half function whenever this IRQ gets triggered i.e. `arduino_irq_bottom_half()`.

This IRQ can be triggered with external hardware or `gpioset`. Upon success, the top-half handler print out message to log:

```
[ 1820.715887] arduino_irq arduino: irq=43 top-half executed
```

and then hands bottom-half to dedicated IRQ threaded, which executes  `arduino_irq_bottom_half()` and prints another message to log:

```
[ 1820.715916] arduino_irq arduino: irq=43 bottom-half executed
```

## What to look into?


### 1. Dedicated bottom-half thread being created

After install this module, a dedicated bottom-half thread will be created, which can be observed with `ps -aux`:

```diff
 $ ps -aux | grep irq
 root           9  0.0  0.0      0     0 ?        S    06:33   0:00 [ksoftirqd/0]
 root          18  0.0  0.0      0     0 ?        S    06:33   0:00 [ksoftirqd/1]
 root          24  0.0  0.0      0     0 ?        S    06:33   0:00 [ksoftirqd/2]
 root          30  0.0  0.0      0     0 ?        S    06:33   0:00 [ksoftirqd/3]
 root         105  0.0  0.0      0     0 ?        S    06:33   0:00 [irq/41-aerdrv]
 root         157  0.0  0.0      0     0 ?        S    06:33   0:00 [irq/28-mmc0]
 root        1682  0.0  0.0  80932  3028 ?        Ssl  06:34   0:00 /usr/sbin/irqbalance --foreground
+root        2624  0.0  0.0      0     0 ?        S    06:43   0:00 [irq/43-arduino]
 ubuntu      2632  0.0  0.0   7692   648 pts/0    S+   06:44   0:00 grep --color=auto irq
```

### 2. Bottom-half being executed by dedicated thread

tracing with the same way:

```
$ sudo trace-cmd record \
	-p function_graph \
	--max-graph-depth 5 \
	-g arduino_irq_top_half \
	-g arduino_irq_bottom_half \
	&
```

under some `ping` loads:

```
$ sudo ping -i 0.0001 172.20.10.8 &> /dev/null &
```

and trigger it a few times with Arduino or `gpioset`:

```
$ sudo gpioset gpiochip0 17=1
```

after generating report with `-l` option:

```
$ trace-cmd report -l | less
```

you'll see something like this:

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

The behavior of top-half handler remains the same as previous examples. It could interrupt `idle`, `ping`, `gpioset` if you trigger this IRQ with it, or even `ksoftirqd`. However, the bottom-half handler is always executed by dedicated `irq/43-arduino-2624` thread, which is created by `request_threaded_irq()` function upon probe. Also, The latency format shows that this bottom-half handler runs in process context (`.`), which comes as no surprise since this handler is run by process `irq/43-arduino` (PID 2624).

You can also replace `trace-cmd` command with stack trace:

```
$ sudo trace-cmd record \
	-p function \
	--func-stack \
	-l arduino_irq_top_half \
	-l arduino_irq_bottom_half \
	&
```

which shows how the dedicated IRQ thread gets invoked:

```c
irq/43-a-2624    1...1  1683.740263: function:             arduino_irq_bottom_half
irq/43-a-2624    1...1  1683.740266: kernel_stack:         <stack trace>
=> ftrace_graph_call (ffffc6f26369f7a8)
=> arduino_irq_bottom_half (ffffc6f217ddc118)
=> irq_thread_fn (ffffc6f26376bfb4)
=> irq_thread (ffffc6f26376c930)
=> kthread (ffffc6f26371ce80)
=> ret_from_fork (ffffc6f263687090)
```

## Trivia

Compared to original hard IRQ mechanism, where one interrupt must get finished first before another one (which may be more emergent) get served:

![](https://i.imgur.com/OEKhZPQ.jpg)

threaded IRQs reduce latency between "an emergent hardware event happens" and "an emergent hardware event gets served" (indicated by gray double-sided arrow), because 1) all the top-half handler has to do is to wake up the dedicated IRQ thread and 2) scheduler now gets to decide which IRQ to handle first, by means of scheduling policies and preemption since they are all threads now. Thus, interrupts with greater ermengency can now preempt the ones with lower priority:

![](https://i.imgur.com/RiVlcz5.jpg)

Also, there's no need to fill out work-representing data structure (e.g. `work_struct`, `tasklet_struct`) if threaded IRQ is used, so it also simplifies the code.

## Where to go next?

Look up [*What Every Driver Developer Should Know about RT*](https://youtu.be/-J0y_usjYxo) given by *Julia Cartwright* in ELCE 2018 to get more detail about threaded IRQ.  Also, [*IRQs: the Hard, the Soft, the Threaded and the Preemptible*](https://youtu.be/-pehAzaP1eg) given by *Alison Chaiken* on ELCE 2016 has brief introduction about threaded IRQ. [*Real Time is Coming to Linux; What Does that Mean to You?*](https://youtu.be/BxJm-Ujipcg), or literally any event where *Steven Rostedt* talk about `PREEMPT_RT`, talked about threaded IRQ as well.
