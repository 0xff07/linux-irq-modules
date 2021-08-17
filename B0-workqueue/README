# `B0-Workqueue`

> Make sure you already applied devicetree overlay in `A0-interrupt-dt` before getting started.

[TOC]

## What is it?

It's a kernel module registering rising-edge event of `GPIO17` on Raspberry Pi 4B as an Linux IRQ, with **workqueue** serving its bottom-half.

## How to use it?

`make` it:

```
$ make
```

and install it with `insmod`:

```
$ sudo insmod arduino-irq.ko
```

if succeeded, you'll see following message in `dmesg`:

```
[ 1590.287763] arduino_irq arduino: successfully probed arduino!
```

Then you can poke it with external hardware or `gpioset`:

```
$ sudo gpioset gpiochip0 17=1
```

On success, the following messages will be printed to log:

```
[ 1660.501015] arduino_irq arduino: irq=43 top-half executed
[ 1660.501030] arduino_irq arduino: irq=43 bottom-half executed
```

## How does it work?

In `probe()`, this kernel module registers `arduino_irq_top_half()`  as a top-half handler for rising-edge event of `GPIO17`. Apart from printing message:

```
[ 1660.501015] arduino_irq arduino: irq=43 top-half executed
```

this top-half handler also queues a `work_struct`, initialized by `INIT_WORK()` in `probe()` beforehand, to global workqueue by `schedule_work()` as "bottom-half" of this IRQ. When `kworker` thread runs this partiluar `work_struct`, `arduino_irq_bottom_half()` is executed, printing out following message to log:

```
[ 1660.501030] arduino_irq arduino: irq=43 bottom-half executed
```


## What to look into?

Use `trace-cmd` on both top-half and bottom-half functions (See tracing steps in top-level README):

```
$ sudo trace-cmd record \
	-p function_graph \
	--max-graph-depth 3 \
	-g arduino_irq_top_half \
	-g arduino_irq_bottom_half \
	&
```

under some work loads in the background, say:

```
$ sudo ping -i ubnutu &> /dev/null &
```

then after poking it a few time with Arduino or `gpioset` and checking `trace-cmd` report with `-l` option, you'll see some interesting things:

### 1. Top-half trying to queue work

First you'll see this top-half handler trying to queue work, which should come as no surprise:

```
  <idle>-0       0d.h1  1812.941255: funcgraph_entry:                   |  arduino_irq_top_half() {
  <idle>-0       0d.h1  1812.941259: funcgraph_entry:                   |    queue_work_on() {
  <idle>-0       0d.h1  1812.941260: funcgraph_entry:        9.611 us   |      __queue_work();
  <idle>-0       0dNh1  1812.941271: funcgraph_exit:       + 11.945 us  |    }
  <idle>-0       0dNh1  1812.941272: funcgraph_entry:                   |    _dev_info() {
  <idle>-0       0dNh1  1812.941273: funcgraph_entry:      + 18.166 us  |      __dev_printk();
  <idle>-0       0dNh1  1812.941291: funcgraph_exit:       + 19.519 us  |    }
  <idle>-0       0dNh1  1812.941292: funcgraph_exit:       + 38.019 us  |  }
```

Like the previous example, this top-half handler may interrupt different processes, say `ping`:


```
    ping-3106    0d.H2  2187.576739: funcgraph_entry:                   |  arduino_irq_top_half() {
    ping-3106    0d.H2  2187.576743: funcgraph_entry:                   |    queue_work_on() {
    ping-3106    0d.H2  2187.576744: funcgraph_entry:      + 13.371 us  |      __queue_work();
    ping-3106    0dNH2  2187.576759: funcgraph_exit:       + 15.648 us  |    }
    ping-3106    0dNH2  2187.576760: funcgraph_entry:                   |    _dev_info() {
    ping-3106    0dNH2  2187.576761: funcgraph_entry:      + 17.629 us  |      __dev_printk();
    ping-3106    0dNH2  2187.576779: funcgraph_exit:       + 18.945 us  |    }
    ping-3106    0dNH2  2187.576780: funcgraph_exit:       + 41.518 us  |  }
```

or `gpioset` if you use it to trigger this IRQ:

```
 gpioset-3124    0d.h1  2179.117113: funcgraph_entry:                   |    arduino_irq_top_half() {
 gpioset-3124    0d.h1  2179.117117: funcgraph_entry:      + 16.018 us  |      queue_work_on();
 gpioset-3124    0dNh1  2179.117135: funcgraph_entry:      + 17.167 us  |      _dev_info();
 gpioset-3124    0dNh1  2179.117152: funcgraph_exit:       + 40.574 us  |    }
```

### 2. `work_struct` being done by `kworker` thread

While top-half handler (i.e. the function registered by  one of the `request_irq()` functions) is always executed in hard IRQ context, different bottom-half mechanisms may run in different context. For workqueue, the `work_struct` is done by dedicated kernel threads called `kworker`, and thus always runs in task context (also called process context, thread context... the list goes on. They are all `task_struct` anyway). 

This can be verified by reading latency format in `trace-cmd` report, which shows bottom-half function in this case being executed in process context (`.`) by `kworker`:

```
kworker/-2811    0....  1812.941309: funcgraph_entry:                   |  arduino_irq_bottom_half() {
kworker/-2811    0....  1812.941310: funcgraph_entry:                   |    _dev_info() {
kworker/-2811    0....  1812.941312: funcgraph_entry:      + 10.463 us  |      __dev_printk();
kworker/-2811    0....  1812.941323: funcgraph_exit:       + 12.704 us  |    }
kworker/-2811    0....  1812.941324: funcgraph_exit:       + 15.204 us  |  }
```

It's not always done by the same `kworker` though:

```
kworker/-3071    0....  2193.893326: funcgraph_entry:                   |  arduino_irq_bottom_half() {
kworker/-3071    0....  2193.893327: funcgraph_entry:                   |    _dev_info() {
kworker/-3071    0....  2193.893329: funcgraph_entry:      + 11.611 us  |      __dev_printk();
kworker/-3071    0....  2193.893341: funcgraph_exit:       + 13.778 us  |    }
kworker/-3071    0....  2193.893342: funcgraph_exit:       + 16.408 us  |  }
```

### 3. Stack trace when `kworker` processing `work_struct`

If `function` tracer is used instead of `function_graph` tracer (see instructions in top-level README):

```
$ sudo trace-cmd record \
	-p function \
	--func-stack \
	-l arduino_irq_top_half \
	-l arduino_irq_bottom_half \
	&
```

You can see how `work_struct` is done by `kworker`:

```
kworker/-2605    0...1  2357.727961: function:             arduino_irq_bottom_half
kworker/-2605    0...1  2357.727963: kernel_stack:         <stack trace>
=> ftrace_graph_call (ffffa31c3f09f7a8)
=> arduino_irq_bottom_half (ffffa31c02ff61c8)
=> process_one_work (ffffa31c3f115be4)
=> worker_thread (ffffa31c3f115ed4)
=> kthread (ffffa31c3f11ce80)
=> ret_from_fork (ffffa31c3f087090)
```

## Trivia

There are tracepoints for workqueue as well. A quick look on `bpftrace -l` gives you these tracepoints:

```
...
tracepoint:workqueue:workqueue_queue_work
tracepoint:workqueue:workqueue_activate_work
tracepoint:workqueue:workqueue_execute_start
tracepoint:workqueue:workqueue_execute_end
...
```

To find where a tracepoint is placed, search function name of the form `trace_<name>` , where `<name>` is the last segment of the name of a tracepoint. For examle, `workqueue_execute_start` tracepoint lies at [`process_one_work()`](https://elixir.bootlin.com/linux/latest/source/kernel/workqueue.c#L2275) in `kernel/workqueue.c` where `trace_workqueue_execute_start()` is called.

> Elixir of Bootlin dosen't always keep track these funtions, but `cscope` finds it immediately.

## Where to go next?

Check [*Concurrency Managed Workqueue (cmwq)*](https://www.kernel.org/doc/html/latest/core-api/workqueue.html) for detail about workqueue.
