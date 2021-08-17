# `B1-tasklet`

> Make sure you already applied devicetree overlay in `00-interrupt-dt` before getting started.

[TOC]

## What is it?

It's a kernel module registering rising-edge event of `GPIO17` on Raspberry Pi 4B as a Linux IRQ, with **tasklet** serving its bottom-half.

## How to use it?

After building:

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

Upon success, this kernel module prints out following message to log, which can be viewed by `dmesg`:

```
[   99.629801] arduino_irq arduino: irq=43 top-half executed
[   99.629829] arduino_irq arduino: irq=43 bottom-half executed
```

## How does it work?

Upon successful driver match, inside `probe()` function this kernel module registers `arduino_irq_top_half()` as a top-half handler for rising-edge event of `GPIO17`. Apart from printing message:

```
[   99.629801] arduino_irq arduino: irq=43 top-half executed
```

this top-half handler also submits a `tasklet_struct`, also initialized in `probe()` with `tasklet_init()` beforehand, by calling `tasklet_schedule()` as "bottom-half" of this IRQ. When this particular`tasklet_struct` runs,  `arduino_irq_bottom_half()`  is executed, printing out following message to log:

```
[   99.629829] arduino_irq arduino: irq=43 bottom-half executed
```


## What to look into?

One difference between tasklets and workqueues is that tasklets are a type of **soft IRQ**. To see the difference, use `function_graph` tracing on both top-half and bottom-half functions (see instructions in top-level README):

```
$ sudo trace-cmd record \
	-p function_graph \
	--max-graph-depth 5 \
	-g arduino_irq_top_half \
	-g arduino_irq_bottom_half \
	&
```

with `ping` flooding in background:

```
$ sudo ping -i 0.0001 ubuntu &> /dev/null &
```

and then poke the IRQ for a few times with Arduino or `gpioset`:

```
$ sudo gpioset gpiochip0 17=1
```

Finally, view `trace-cmd` report with `-l` option after moving both `ping` and `trace-cmd` to foreground with `fg` and stop them with Ctrl + C:

```
$ trace-cmd report -l | less
```

### 1. Top-half handler queueing tasklet

This should come as no surprise, since it's literally how this top-half handler is implemented:

```
  <idle>-0       0d.h1  7348.538660: funcgraph_entry:                   |  arduino_irq_top_half() {
  <idle>-0       0d.h1  7348.538664: funcgraph_entry:                   |    __tasklet_schedule() {
  <idle>-0       0d.h1  7348.538665: funcgraph_entry:                   |      __tasklet_schedule_common() {
  <idle>-0       0d.h1  7348.538666: funcgraph_entry:        0.796 us   |        __raise_softirq_irqoff();
  <idle>-0       0d.h1  7348.538668: funcgraph_exit:         2.926 us   |      }
  <idle>-0       0d.h1  7348.538669: funcgraph_exit:         4.408 us   |    }
  <idle>-0       0d.h1  7348.538669: funcgraph_entry:                   |    _dev_info() {
  <idle>-0       0d.h1  7348.538670: funcgraph_entry:                   |      __dev_printk() {
  <idle>-0       0d.h1  7348.538672: funcgraph_entry:                   |        dev_printk_emit() {
  <idle>-0       0d.h1  7348.538675: funcgraph_entry:      + 17.000 us  |          dev_vprintk_emit();
  <idle>-0       0d.h1  7348.538693: funcgraph_exit:       + 21.870 us  |        }
  <idle>-0       0d.h1  7348.538693: funcgraph_exit:       + 23.334 us  |      }
  <idle>-0       0d.h1  7348.538694: funcgraph_exit:       + 24.556 us  |    }
  <idle>-0       0d.h1  7348.538694: funcgraph_exit:       + 35.963 us  |  }
```

and as usual, sometimes it interrupts other processes, say `ping`:

```
    ping-3744    0d.h.  7350.626061: funcgraph_entry:                   |  arduino_irq_top_half() {
    ping-3744    0d.h.  7350.626065: funcgraph_entry:                   |    __tasklet_schedule() {
    ping-3744    0d.h.  7350.626066: funcgraph_entry:                   |      __tasklet_schedule_common() {
    ping-3744    0d.h.  7350.626067: funcgraph_entry:        0.870 us   |        __raise_softirq_irqoff();
    ping-3744    0d.h.  7350.626069: funcgraph_exit:         3.019 us   |      }
    ping-3744    0d.h.  7350.626070: funcgraph_exit:         4.426 us   |    }
    ping-3744    0d.h.  7350.626071: funcgraph_entry:                   |    _dev_info() {
    ping-3744    0d.h.  7350.626071: funcgraph_entry:                   |      __dev_printk() {
    ping-3744    0d.h.  7350.626072: funcgraph_entry:                   |        dev_printk_emit() {
    ping-3744    0d.h.  7350.626073: funcgraph_entry:      + 16.482 us  |          dev_vprintk_emit();
    ping-3744    0d.h.  7350.626090: funcgraph_exit:       + 17.797 us  |        }
    ping-3744    0d.h.  7350.626091: funcgraph_exit:       + 19.278 us  |      }
    ping-3744    0d.h.  7350.626091: funcgraph_exit:       + 20.648 us  |    }
    ping-3744    0d.h.  7350.626092: funcgraph_exit:       + 32.000 us  |  }
```

Also, top-half handler runs in hard IRQ context (`h`), like the previous ones. This should come as no surprise as well since they are registered in the same way.

### 2. `tasklet` running in soft IRQ context

Tasklet is a type of soft IRQ, so now the latency format (`0..s.`) shows the bottom-half handler executed in soft IRQ context (`s`):

```
    ping-3744    0..s.  7350.626098: funcgraph_entry:                   |  arduino_irq_bottom_half() {
    ping-3744    0..s.  7350.626099: funcgraph_entry:                   |    _dev_info() {
    ping-3744    0..s.  7350.626100: funcgraph_entry:                   |      __dev_printk() {
    ping-3744    0..s.  7350.626102: funcgraph_entry:                   |        dev_printk_emit() {
    ping-3744    0..s.  7350.626103: funcgraph_entry:      + 10.130 us  |          dev_vprintk_emit();
    ping-3744    0..s.  7350.626114: funcgraph_exit:       + 12.296 us  |        }
    ping-3744    0..s.  7350.626116: funcgraph_exit:       + 14.574 us  |      }
    ping-3744    0..s.  7350.626118: funcgraph_exit:       + 18.278 us  |    }
    ping-3744    0..s.  7350.626119: funcgraph_exit:       + 20.778 us  |  }
```

### 3. `ksoftirqd` trying to help

If soft IRQ floods really hard, dedicated `ksoftirqd` daemons will be woken up to help ease the pain:

```
ksoftirq-9       0..s.  8436.060536: funcgraph_entry:                   |  arduino_irq_bottom_half() {
ksoftirq-9       0..s.  8436.060539: funcgraph_entry:                   |    _dev_info() {
ksoftirq-9       0..s.  8436.060540: funcgraph_entry:                   |      __dev_printk() {
ksoftirq-9       0..s.  8436.060542: funcgraph_entry:                   |        dev_printk_emit() {
ksoftirq-9       0..s.  8436.060543: funcgraph_entry:      + 17.352 us  |          dev_vprintk_emit();
ksoftirq-9       0..s.  8436.060562: funcgraph_exit:       + 20.074 us  |        }
ksoftirq-9       0..s.  8436.060563: funcgraph_exit:       + 22.445 us  |      }
ksoftirq-9       0..s.  8436.060564: funcgraph_exit:       + 24.723 us  |    }
ksoftirq-9       0..s.  8436.060565: funcgraph_exit:       + 30.185 us  |  }
```

## Hard IRQs strike, soft IRQs ambush

Compared to hard IRQs, which strike CPU in an unpredictable manner, a soft IRQ works like an ambush. A soft IRQ has to first get laid or "raised" by, say, `raise_softirq()` or `raise_softirq_irqoff()`. Then, if any unlucky processor walks into certain places, like functions with a `__do_softirq()` buried inside, the pending soft IRQs will bite the processor, drawing its attention to work on soft IRQ instead of what it's supposed to do. 

Some notable places where a processor may get ambushed by a soft IRQ includes: 

1. Exiting from a hard IRQ; and
2. Re-enabling soft IRQ. 

For the first case, see implementation of [`irq_exit()`](https://elixir.bootlin.com/linux/v5.4.139/source/kernel/softirq.c#L403) in `kernel/softirq.c`, in which [`invoke_softirq()`](https://elixir.bootlin.com/linux/v5.4.139/source/kernel/softirq.c#L361) calls `__do_softirq()`; for the other one, see implementation of softirq-enabling functions like [`local_bh_enable()`](https://elixir.bootlin.com/linux/v5.4.139/source/include/linux/bottom_half.h#L30), which eventually calls [`__local_bh_enable_ip()`](https://elixir.bootlin.com/linux/v5.4.139/source/kernel/softirq.c#L166) in `kernel/softirq.c` and thus `__do_softirq()`.

Also, if soft IRQ floods too hard, `ksoftirqd`s will be woken up to handle soft IRQs.

> Note that for historical reason, `bh` (stands for *bottom half*) is used, instead of something like `softirq`, when it comes to naming functions relating to soft IRQ.

## What does `ftrace` say?

Besides reasoning from source code, these facts can also be verified by dynamic tracing. Simply replace `function_graph` tracer by `function` tracer and repeat the same things:

```shell
$ sudo trace-cmd record \
	-p function \
	--func-stack \
	-l arduino_irq_top_half \
	-l arduino_irq_bottom_half \
	&
```

add some `ping` flood:

```
$ sudo ping -i 0.0001 ubuntu &> /dev/null &
```

poke it for a few time with Arduino or `gpioset`:

```
$ sudo gpioset gpiochip0 17=1
```

Finally move both `ping` and `trace-cmd` back to foreground with `fg` and end them before checking what happen with `trace-cmd`'s report command:

```
$ trace-cmd report -l | less
```

Then you can see both cases happen in stack trace:

### Case 1. Soft IRQ hits after a hard IRQ

This is the case where a `__do_softirq()` being called inside a `irq_exit()`. You'll see pattern like this:

```diff
          <idle>-0     [000]  7705.408362: function:             arduino_irq_bottom_half
          <idle>-0     [000]  7705.408364: kernel_stack:         <stack trace>
=> ftrace_graph_call (ffffa31c3f09f7a8)
=> arduino_irq_bottom_half (ffffa31c02ff6240)
=> tasklet_action_common.isra.0 (ffffa31c3f0f8230)
=> tasklet_action (ffffa31c3f0f8348)
+=> __do_softirq (ffffa31c3f081cd8)
+=> irq_exit (ffffa31c3f0f7ffc)
=> __handle_domain_irq (ffffa31c3f16a2ac)
=> gic_handle_irq (ffffa31c3f08181c)
=> el1_irq (ffffa31c3f083788)
=> arch_cpu_idle (ffffa31c3f0892f0)
=> default_idle_call (ffffa31c3fbbebd8)
=> do_idle (ffffa31c3f133744)
=> cpu_startup_entry (ffffa31c3f133a00)
=> rest_init (ffffa31c3fbb7d1c)
=> arch_call_rest_init (ffffa31c40200c7c)
=> start_kernel (ffffa31c402011ec)
```

Since in this scenario soft IRQ is executed right after a hard IRQ, and a hard IRQ may interrupt arbitrary processes, a soft IRQ may also interrupt arbitrary process. For example, sometimes you can see  `ping` being the process behind this soft IRQ:

```diff
+   ping-4211    0..s2  8960.501664: function:             arduino_irq_bottom_half
+   ping-4211    0..s2  8960.501665: kernel_stack:         <stack trace>
=> ftrace_graph_call (ffffa31c3f09f7a8)
=> arduino_irq_bottom_half (ffffa31c02ff6240)
=> tasklet_action_common.isra.0 (ffffa31c3f0f8230)
=> tasklet_action (ffffa31c3f0f8348)
+=> __do_softirq (ffffa31c3f081cd8)
+=> irq_exit (ffffa31c3f0f7ffc)
=> __handle_domain_irq (ffffa31c3f16a2ac)
=> gic_handle_irq (ffffa31c3f08181c)
=> el1_irq (ffffa31c3f083788)
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
=> el0_svc (ffffa31c3f084690)
```

and if you use `gpioset` to trigger this IRQ, sometimes you see soft IRQ interrupting `gpioset`:

```diff
 gpioset-4237    0..s1  8962.217080: function:             arduino_irq_bottom_half
 gpioset-4237    0..s1  8962.217082: kernel_stack:         <stack trace>
=> ftrace_graph_call (ffffa31c3f09f7a8)
=> arduino_irq_bottom_half (ffffa31c02ff6240)
=> tasklet_action_common.isra.0 (ffffa31c3f0f8230)
=> tasklet_action (ffffa31c3f0f8348)
+=> __do_softirq (ffffa31c3f081cd8)
+=> irq_exit (ffffa31c3f0f7ffc)
=> __handle_domain_irq (ffffa31c3f16a2ac)
=> gic_handle_irq (ffffa31c3f08181c)
=> el1_irq (ffffa31c3f083788)
=> mutex_unlock (ffffa31c3fbbb29c)
=> pinctrl_gpio_direction (ffffa31c3f604d98)
=> pinctrl_gpio_direction_output (ffffa31c3f604e10)
=> bcm2835_gpio_direction_output (ffffa31c3f60f674)
=> gpiod_direction_output_raw_commit (ffffa31c3f614dc4)
=> gpiod_direction_output (ffffa31c3f6151dc)
=> linehandle_create (ffffa31c3f616334)
=> gpio_ioctl (ffffa31c3f616958)
=> do_vfs_ioctl (ffffa31c3f35151c)
=> ksys_ioctl (ffffa31c3f3517a0)
=> __arm64_sys_ioctl (ffffa31c3f3517fc)
=> el0_svc_common.constprop.0 (ffffa31c3f09bd7c)
=> el0_svc_handler (ffffa31c3f09bf48)
=> el0_svc (ffffa31c3f084690)
```

### Case 2. Soft IRQ hits after getting re-enabled

Another scenario for soft IRQ to run is when bh-enabling functions are called. In this case you see pattern like this:

```diff
    ping-4211    0..s2  8961.858024: function:             arduino_irq_bottom_half
    ping-4211    0..s2  8961.858026: kernel_stack:         <stack trace>
=> ftrace_graph_call (ffffa31c3f09f7a8)
=> arduino_irq_bottom_half (ffffa31c02ff6240)
=> tasklet_action_common.isra.0 (ffffa31c3f0f8230)
=> tasklet_action (ffffa31c3f0f8348)
+=> __do_softirq (ffffa31c3f081cd8)
+=> do_softirq.part.0 (ffffa31c3f0f7dd0)
+=> __local_bh_enable_ip (ffffa31c3f0f7e9c)
=> ip_finish_output2 (ffffa31c3fa7aefc)
=> __ip_finish_output (ffffa31c3fa7baec)
=> ip_finish_output (ffffa31c3fa7bc5c)
=> ip_output (ffffa31c3fa7c8d4)
=> ip_local_out (ffffa31c3fa7bef4)
=> ip_send_skb (ffffa31c3fa7d298)
=> ip_push_pending_frames (ffffa31c3fa7d370)
=> ping_v4_sendmsg (ffffa31c3fad8fe4)
=> inet_sendmsg (ffffa31c3fac4c00)
=> sock_sendmsg (ffffa31c3f9bdc20)
=> __sys_sendto (ffffa31c3f9c3310)
=> __arm64_sys_sendto (ffffa31c3f9c33b4)
=> el0_svc_common.constprop.0 (ffffa31c3f09bd7c)
=> el0_svc_handler (ffffa31c3f09bf48)
=> el0_svc (ffffa31c3f084690)
```

## More on soft IRQ

### Soft IRQ handlers and where to find them

Soft IRQs are essentially a statically defined handler array initialized by `open_softirq()`. If you clone kernel source code, `make cscope`, and search where `open_softirq()` is called (by `cscope`), you will see corresponding handler for each soft IRQ being registered:

```c
Functions calling this function: open_softirq

  File       Function              Line
0 blk-mq.c   blk_mq_init            4024 open_softirq(BLOCK_SOFTIRQ, blk_done_softirq);
1 tiny.c     rcu_init                222 open_softirq(RCU_SOFTIRQ, rcu_process_callbacks);
2 tree.c     rcu_init               4757 open_softirq(RCU_SOFTIRQ, rcu_core_si);
3 fair.c     init_sched_fair_class 11574 open_softirq(SCHED_SOFTIRQ, run_rebalance_domains);
4 softirq.c  softirq_init            903 open_softirq(TASKLET_SOFTIRQ, tasklet_action);
5 softirq.c  softirq_init            904 open_softirq(HI_SOFTIRQ, tasklet_hi_action);
6 hrtimer.c  hrtimers_init          2119 open_softirq(HRTIMER_SOFTIRQ, hrtimer_run_softirq);
7 timer.c    init_timers            2020 open_softirq(TIMER_SOFTIRQ, run_timer_softirq);
8 irq_poll.c irq_poll_setup          210 open_softirq(IRQ_POLL_SOFTIRQ, irq_poll_softirq);
9 dev.c      net_dev_init          11674 open_softirq(NET_TX_SOFTIRQ, net_tx_action);
a dev.c      net_dev_init          11675 open_softirq(NET_RX_SOFTIRQ, net_rx_action);
```

Also, `open_softirq()` involves initializing a statically defined soft IRQ array, defined in [`kernel/softirq.c`](https://elixir.bootlin.com/linux/latest/source/kernel/softirq.c#L59):

```c
static struct softirq_action softirq_vec[NR_SOFTIRQS] __cacheline_aligned_in_smp;

...

void open_softirq(int nr, void (*action)(struct softirq_action *))
{
	softirq_vec[nr].action = action;
}
```

which means that it's unlikely to add new type of soft IRQ without modifying and recompiling kernel source code.

Note that the seemingly duplicated `RCU_SOFTIRQ` initialization in `rcu_init()` comes from different implementations of RCU. Only one of them will be registered, depending on kernel configuration. See `#ifdef`s in [`include/linux/rcupdate.h`](https://elixir.bootlin.com/linux/latest/source/include/linux/rcupdate.h#L224).

### Priority of soft IRQs

When a CPU bumps into places that trigger soft IRQ, it's not uncommon that multiple soft IRQs are pending at the same time. In this case, the CPU does each pending soft IRQ handler once, according do priority defined in enum in [`include/linux/interrupt.h`](https://elixir.bootlin.com/linux/latest/source/include/linux/interrupt.h):

```c
enum
{
	HI_SOFTIRQ=0,
	TIMER_SOFTIRQ,
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	BLOCK_SOFTIRQ,
	IRQ_POLL_SOFTIRQ,
	TASKLET_SOFTIRQ,
	SCHED_SOFTIRQ,
	HRTIMER_SOFTIRQ,
	RCU_SOFTIRQ,    /* Preferable RCU should always be the last softirq */

	NR_SOFTIRQS
};
```

See implementation of [`__do_softirq()`](https://elixir.bootlin.com/linux/latest/source/kernel/softirq.c#L547) in `kernel/softirq.c`.

### Don't sleep in soft IRQ

Similar to the case of hard IRQ, a soft IRQ may interrupt arbitrary process, so calling functions involving recheduling or blocking will send innocent process interrupted by soft IRQ to sleep, instead of doing delay in soft IRQ, and is considered a bug. In other words, soft IRQ context is also an atomic context. 

## Where to go next?

[*IRQs: the Hard, the Soft, the Threaded and the Preemptible*](https://youtu.be/-pehAzaP1eg) given by *Alison Chaiken* on ELCE 2016 also talk about soft IRQ, especially about how and when a soft IRQ get handled.

Also, have a tour on comments in [`include/linux/interrupt.h`](https://elixir.bootlin.com/linux/latest/source/include/linux/interrupt.h). For example the one about [soft IRQ](https://elixir.bootlin.com/linux/latest/source/include/linux/interrupt.h#L535):

```
/* PLEASE, avoid to allocate new softirqs, if you need not _really_ high
   frequency threaded job scheduling. For almost all the purposes
   tasklets are more than enough. F.e. all serial device BHs et
   al. should be converted to tasklets, not to softirqs.
 */
```

or the one on [`struct tasklet_struct`](https://elixir.bootlin.com/linux/latest/source/include/linux/interrupt.h#L590):

```
/* Tasklets --- multithreaded analogue of BHs.

   This API is deprecated. Please consider using threaded IRQs instead:
   https://lore.kernel.org/lkml/20200716081538.2sivhkj4hcyrusem@linutronix.de

   Main feature differing them of generic softirqs: tasklet
   is running only on one CPU simultaneously.

   Main feature differing them of BHs: different tasklets
   may be run simultaneously on different CPUs.

   ...
 */
```

*Thomas Gleixner* also had some words about `tasklet` on [*Ask the Expert Session*](https://youtu.be/OABEFSQERXM?t=582) held by Linux Foundation.
