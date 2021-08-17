# `A1-request-irq`

> Make sure you have applied devicetree overlay in `A0-interrupt-dt` before getting started.

[TOC]

## What is it?

It's a kernel module that registers rising edge on `GPIO17` of Raspberry Pi 4B (specified in its devicetree node) as a Linux IRQ. This particular IRQ can be triggered by `gpioset`, or by external hardware. 

When triggered, the registered handler prints out message to log:

```
[ 4292.618289] arduino_irq arduino: irq=43 top-half executed
```

## How to use it?

After building 

```
$ make
```

and installing:

```
$ sudo insmod arduino-irq.ko
```

you can either setup Arduino hardware, as in top level README, or use `gpioset` to trigger this IRQ:

```
$ sudo gpioset gpiochip0 17=1
```

Upon success, this kernel module prints message to log, which can be viewed by `dmesg`

```
[ 4292.618289] arduino_irq arduino: irq=43 top-half executed
```

Note that the IRQ number may change, depending on how many IRQs are registered in current system.

## How does it work?

### Driver-matching aspects

When this module gets installed, `platform_driver` declared in this module will be registered to a bus called "platform bus" during module initialization (see implementation and comment of [`module_platform_driver()`](https://elixir.bootlin.com/linux/latest/source/include/linux/platform_device.h#L256)). 

After the `platform_driver` get registered, the "platform bus" will try to find devices this driver can serve by comparing `compatible` member of entries of driver's `of_match_table`, which is a C-string, and `compatible` strings provided in devicetree nodes. If there is a match, the driver and device are paired, and `probe()` function in registered `platform_driver` structure is called to do initialization.

### IRQ-registering aspects

The `probe()` function first claims interrupt specified in devicetree node with `platform_get_irq()`. This function returns an IRQ number, which is the abstraction of an interrupt in kernel's point of view. After that, `arduino_irq_top_half()` is registered as a top-half handler of this IRQ by `devm_request_irq()`, the "managed" version of `request_irq()` (it will free this IRQ automatically upon driver detached). This top-half handler prints out message to log when triggered.


## What to look into?

Beside poking with Arduino or `gpioset`, you can see how IRQ mechanisms work inside Linux kernel by `trace-cmd`. For example, you can record what top-half function does by `function_graph` tracer:

```
$ sudo trace-cmd record \
	-p function_graph \
	--max-graph-depth 5 \
	-g arduino_irq_top_half \
	&
```

Then trigger it a few times with Arduino or `gpioset`:

```
$ sudo gpioset gpiochip0 17=1
```

After putting `trace-cmd` back to foreground with `fg` and stop it, generate report with `-l` option, which shows additional "latency format" in report:

```
$ sudo trace-cmd report -l | less
```

### 1. Top-Half being Executed in Hard IRQ Context

You'll see the following pattern appears repeatedly in report:

```c
  <idle>-0       0d.h1  1012.314456: funcgraph_entry:                   |  arduino_irq_top_half() {
  <idle>-0       0d.h1  1012.314459: funcgraph_entry:                   |    _dev_info() {
  <idle>-0       0d.h1  1012.314460: funcgraph_entry:                   |      __dev_printk() {
  <idle>-0       0d.h1  1012.314462: funcgraph_entry:                   |        dev_printk_emit() {
  <idle>-0       0d.h1  1012.314462: funcgraph_entry:      + 17.056 us  |          dev_vprintk_emit();
  <idle>-0       0d.h1  1012.314481: funcgraph_exit:       + 19.240 us  |        }
  <idle>-0       0d.h1  1012.314481: funcgraph_exit:       + 21.074 us  |      }
  <idle>-0       0d.h1  1012.314482: funcgraph_exit:       + 22.648 us  |    }
  <idle>-0       0d.h1  1012.314483: funcgraph_exit:       + 28.296 us  |  }
```

Latency format (`0d.h1`) reveals information about CPU number, `preempt_count`, whether local IRQ being disable etc. when the piece of code is executed. In particular, it also shows the context where a function is executed. For example, in this case `arduino_irq_top_half()` is executed in hard IRQ context (`h`). 

### 2. Hard IRQ Interrupting Soft IRQ

If you trace it when some load is happening, say, some ping flood:

```
$ sudo ping -i 0.0001 ubuntu &> /dev/null &
```

with the same tracing option:

```
$ sudo trace-cmd record \
	-p function_graph \
	--max-graph-depth 5 \
	-g arduino_irq_top_half \
	&
```

Than after triggering this IRQ a few times with the same way:

```
$ sudo gpioset gpiochip0 17=1
```

besides the `idle` process (the process with PID 0) being interrupted, sometimes you see this handler interrupting `ping`:

```
    ping-2320    0d.H3  1473.090603: funcgraph_entry:                   |  arduino_irq_top_half() {
    ping-2320    0d.H3  1473.090607: funcgraph_entry:                   |    _dev_info() {
    ping-2320    0d.H3  1473.090608: funcgraph_entry:                   |      __dev_printk() {
    ping-2320    0d.H3  1473.090609: funcgraph_entry:                   |        dev_printk_emit() {
    ping-2320    0d.H3  1473.090610: funcgraph_entry:      + 17.407 us  |          dev_vprintk_emit();
    ping-2320    0d.H3  1473.090629: funcgraph_exit:       + 19.759 us  |        }
    ping-2320    0d.H3  1473.090629: funcgraph_exit:       + 21.648 us  |      }
    ping-2320    0d.H3  1473.090630: funcgraph_exit:       + 23.241 us  |    }
    ping-2320    0d.H3  1473.090631: funcgraph_exit:       + 29.222 us  |  }
```

In this case, the latency format shows an `H` instead of `h`, which means that processor enters hard IRQ context from soft IRQ context.

### 3. `ksoftirqd` trying to Help

If you `ping` flood really hard, you'll see top-half handle interrupts `ksoftirqd` trying to ease the pain of flooding soft IRQ raised by network subsystem:

```
ksoftirq-9       0d.H1  1475.880594: funcgraph_entry:                   |    arduino_irq_top_half() {
ksoftirq-9       0d.H1  1475.880598: funcgraph_entry:                   |      _dev_info() {
ksoftirq-9       0d.H1  1475.880599: funcgraph_entry:                   |        __dev_printk() {
ksoftirq-9       0d.H1  1475.880601: funcgraph_entry:      + 17.907 us  |          dev_printk_emit();
ksoftirq-9       0d.H1  1475.880620: funcgraph_exit:       + 20.241 us  |        }
ksoftirq-9       0d.H1  1475.880620: funcgraph_exit:       + 21.908 us  |      }
ksoftirq-9       0d.H1  1475.880621: funcgraph_exit:       + 27.519 us  |    }
```

### 4. Function stack when `idle` being interrupted 

Alternatively, you can trace it with `function` tracer in `trace-cmd` to observe the stack:

```
$ sudo trace-cmd record \
	-p function \
	--func-stack \
	-l arduino_irq_top_half \
	&
```

If report is shown with `-l` option as well, you'll be able to see stack trace with ftrace latency format. For example, if this IRQ is interrupting `idle`, the stack trace may look like this: 

```diff
  <idle>-0       0d.h2  2797.973099: function:             arduino_irq_top_half
  <idle>-0       0d.h2  2797.973105: kernel_stack:         <stack trace>
=> ftrace_graph_call (ffffcf8f99c9f7a8)
=> arduino_irq_top_half (ffffcf8f9b974114)
=> __handle_irq_event_percpu (ffffcf8f99d6ac80)
=> handle_irq_event_percpu (ffffcf8f99d6af10)
=> handle_irq_event (ffffcf8f99d6afc0)
=> handle_edge_irq (ffffcf8f99d6fd00)
=> generic_handle_irq (ffffcf8f99d69a88)
=> bcm2835_gpio_irq_handle_bank (ffffcf8f9a21045c)
=> bcm2835_gpio_irq_handler (ffffcf8f9a210528)
=> generic_handle_irq (ffffcf8f99d69a88)
=> __handle_domain_irq (ffffcf8f99d6a2a8)
=> gic_handle_irq (ffffcf8f99c8181c)
+=> el1_irq (ffffcf8f99c83788)
=> arch_cpu_idle (ffffcf8f99c892f0)
=> default_idle_call (ffffcf8f9a7bebd8)
=> do_idle (ffffcf8f99d33744)
=> cpu_startup_entry (ffffcf8f99d339fc)
=> rest_init (ffffcf8f9a7b7d1c)
=> arch_call_rest_init (ffffcf8f9ae00c7c)
=> start_kernel (ffffcf8f9ae011ec)
```

Similar to the case in `function_graph` tracer, the `h` in latency format (`0d.h2`) indicates that processor is in hard IRQ context upon function entry.

### 5. Function stack when soft IRQ being interrupted

Sometimes our handler interupts a soft IRQ. For example, one possible scenario look like this:

```diff
    ping-2383    0d.H3  2802.515690: function:             arduino_irq_top_half
    ping-2383    0d.H3  2802.515697: kernel_stack:         <stack trace>
=> ftrace_graph_call (ffffcf8f99c9f7a8)
+=> arduino_irq_top_half (ffffcf8f9b974114)
=> __handle_irq_event_percpu (ffffcf8f99d6ac80)
=> handle_irq_event_percpu (ffffcf8f99d6af10)
=> handle_irq_event (ffffcf8f99d6afc0)
=> handle_edge_irq (ffffcf8f99d6fd00)
=> generic_handle_irq (ffffcf8f99d69a88)
=> bcm2835_gpio_irq_handle_bank (ffffcf8f9a21045c)
=> bcm2835_gpio_irq_handler (ffffcf8f9a210528)
=> generic_handle_irq (ffffcf8f99d69a88)
=> __handle_domain_irq (ffffcf8f99d6a2a8)
+=> gic_handle_irq (ffffcf8f99c8181c)
+=> el1_irq (ffffcf8f99c83788)
=> __local_bh_enable_ip (ffffcf8f99cf7e50)
=> ip_finish_output2 (ffffcf8f9a67aefc)
=> __ip_finish_output (ffffcf8f9a67baec)
=> ip_finish_output (ffffcf8f9a67bc5c)
=> ip_output (ffffcf8f9a67c8d4)
=> ip_local_out (ffffcf8f9a67bef4)
=> ip_send_skb (ffffcf8f9a67d298)
=> ip_push_pending_frames (ffffcf8f9a67d370)
=> icmp_push_reply (ffffcf8f9a6bc2f4)
=> icmp_reply.constprop.0 (ffffcf8f9a6bcb04)
=> icmp_echo.part.0 (ffffcf8f9a6bcb70)
=> icmp_echo (ffffcf8f9a6bcbe4)
=> icmp_rcv (ffffcf8f9a6bd260)
=> ip_protocol_deliver_rcu (ffffcf8f9a676218)
=> ip_local_deliver_finish (ffffcf8f9a676434)
=> ip_local_deliver (ffffcf8f9a67656c)
=> ip_rcv_finish (ffffcf8f9a675dfc)
=> ip_rcv (ffffcf8f9a676670)
=> __netif_receive_skb_one_core (ffffcf8f9a5ed728)
=> __netif_receive_skb (ffffcf8f9a5ed7c4)
=> process_backlog (ffffcf8f9a5edaf4)
=> net_rx_action (ffffcf8f9a5efafc)
+=> __do_softirq (ffffcf8f99c81cd8)
=> do_softirq.part.0 (ffffcf8f99cf7dd0)
=> __local_bh_enable_ip (ffffcf8f99cf7e9c)
=> ip_finish_output2 (ffffcf8f9a67aefc)
=> __ip_finish_output (ffffcf8f9a67baec)
=> ip_finish_output (ffffcf8f9a67bc5c)
=> ip_output (ffffcf8f9a67c8d4)
=> ip_local_out (ffffcf8f9a67bef4)
=> ip_send_skb (ffffcf8f9a67d298)
=> ip_push_pending_frames (ffffcf8f9a67d370)
=> ping_v4_sendmsg (ffffcf8f9a6d8fe4)
=> inet_sendmsg (ffffcf8f9a6c4c00)
=> sock_sendmsg (ffffcf8f9a5bdc20)
=> __sys_sendto (ffffcf8f9a5c3310)
=> __arm64_sys_sendto (ffffcf8f9a5c33b4)
=> el0_svc_common.constprop.0 (ffffcf8f99c9bd7c)
=> el0_svc_handler (ffffcf8f99c9bf48)
+=> el0_svc (ffffcf8f99c84690)
```

In this case, CPU emits a system call (`el0_svc`), during which it steps on a soft IRQ (`__do_softirq`) dragging CPU into soft IRQ context, but then interrupted by a hard IRQ (`el1_irq`), which eventually leads to top-half handler registered in our kernel module being executed (`arduino_irq_top_half`). 

If a hard IRQ strikes a processor when it's handling a soft IRQ, an `H` will show up (instead of `h`) in latency format (`0d.H3` in this case) to further emphasize it.

## Trivia

Other than functions inside kernel, there are some fun facts to check:

### 1. GPIO17 get occupied

If you check state of GPIOs by `gpioinfo` after installing the module:

```
$ sudo gpioinfo
```

it will show `GPIO17` being used as a source of interrupt:

```diff
gpiochip0 - 54 lines:
	line   0:     "ID_SDA"       unused   input  active-high
	...
	line  16:     "GPIO16"       unused   input  active-high 
+	line  17:     "GPIO17"  "interrupt"   input  active-high [used]
	line  18:     "GPIO18"       unused   input  active-high 
	...
```

Before the module get installed it would say `GPIO17` is "unused":

```diff
gpiochip0 - 54 lines:
	line   0:     "ID_SDA"       unused   input  active-high
	...
	line  16:     "GPIO16"       unused   input  active-high 
+	line  17:     "GPIO17"       unused   input  active-high 
	line  18:     "GPIO18"       unused   input  active-high
	...
```

### 2. New IRQ registered

If you check `/proc/interrupts` after installing the module, you will see a new IRQ called `arduino` pop up:

```diff
$ cat /proc/interrupts
           CPU0       CPU1       CPU2       CPU3
  1:          0          0          0          0     GICv2  25 Level     vgic
  3:      16694      15719      14913      16179     GICv2  30 Level     arch_timer
  4:          0          0          0          0     GICv2  27 Level     kvm guest vtimer
...
+43:          0          0          0          0  pinctrl-bcm2835  17 Edge      arduino
```

### 3. Tracepoints to hook on

There are some tracepoints for IRQs that can be hooked by eBPF or ftrace. For example `tracepoint:irq:irq_handler_entry` and `tracepoint:irq:irq_handler_exit` in [`kernel/irq/handle.c`](https://elixir.bootlin.com/linux/latest/source/kernel/irq/handle.c#L155). You can hook it with `bpftrace` command, say:

```
$ sudo bpftrace -e 'tracepoint:irq:irq_handler_entry{@[kstack()] = count();}'
```

You can look them up with `bpftrace -l` or `-lv` option.

### 4. Don't sleep in hard IRQ

There are 2 categories of delaying mechanisms:

1. You ask processor to **"do nothing"** for a while. Processor will waste its time doing nothing. One example is `mdelay()`.

2. You ask processor to **"leave this particular process alone"** for a while. A process calling such delay functions is set to sleep state until time is up, and processor can still work on other tasks while that process is sleeping. Like `msleep()`.

If you want to do delay in hard IRQ, use functions of the first kind. Using the other ones does nothing but send innocent processes interrupted by this IRQ to sleep, instead of doing delay for IRQ itself, and is considered a bug.

More generally speaking, calling functions that may implicitly sleep or block in hard IRQ is considered a bug. Kernel docs will explicitly tell you what functions "may sleep", or "must not be called in atomic context". One notable example is the usage of different synchronization primitives. See [*Unreliable Guide To Locking*](https://www.kernel.org/doc/html/latest/kernel-hacking/locking.html).

> Circumstances where calling functions that may implicitly sleep is not allowed are called *atomic context*. A criteria for such context is stated in [`___might_sleep()`](https://elixir.bootlin.com/linux/latest/source/kernel/sched/core.c#L8298).

## Where to go next?

Look up [*IRQs: the Hard, the Soft, the Threaded and the Preemptible*](https://youtu.be/-pehAzaP1eg) given by *Alison Chaiken* on ELCE 2016 to get an idea on different IRQ mechanisms. Also, see [*Using coccinelle to detect (and fix) nested execution context violations*](https://youtu.be/MF8N2m-6dZs) given by *Julia Cartwright* on ELCE 2017 to understand things you shouldn't do during a hard IRQ. For quantitative measurement of interrupt latency, see [*Finding Sources of Latency on your Linux System*](https://youtu.be/Tkra8g0gXAU) given by *Steven Rostedt* on OSS NA 2020.
