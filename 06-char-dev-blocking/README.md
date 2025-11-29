# 06-char-dev-blocking - Blocking I/O with Wait Queues

This example demonstrates blocking I/O operations using Linux wait queues, implementing a producer-consumer pattern.

## What's New: Blocking I/O

### Previous Examples (Non-blocking)
~~~bash
cat /dev/myautodev0
# Returns immediately (reads whatever is in buffer, even if empty)
~~~

### This Example (Blocking)
~~~bash
cat /dev/myblocking
# BLOCKS here, waiting for data...
# (in another terminal) echo "hello" > /dev/myblocking
# Now cat returns with "hello"
~~~

## Key Concepts

### Wait Queues
Wait queues allow processes to sleep until a condition is met:

~~~C
// Declare wait queue
wait_queue_head_t read_queue;

// Initialize in init function
init_waitqueue_head(&read_queue);

// Put process to sleep until condition is true
wait_event_interruptible(read_queue, has_data(dev));

// Wake up sleeping processes
wake_up_interruptible(&read_queue);
~~~

### The Producer-Consumer Pattern

**Readers (Consumers)**:
- Try to read data
- If no data available → sleep on `read_queue`
- When data written → wake up and read

**Writers (Producers)**:
- Write data to buffer
- Wake up any sleeping readers

## Building and Testing

### 1. Build the module

~~~bash
cd 06-char-dev-blocking
make
~~~

### 2. Load in QEMU

~~~bash
./run-qemu.sh
insmod /modules/char-dev-blocking.ko
dmesg | tail
~~~

Output:
~~~
[ 123.456] myblocking: Initializing module...
[ 123.457] myblocking: Allocated major number: 240
[ 123.458] myblocking: Module loaded successfully!
[ 123.459] myblocking: Device /dev/myblocking created
[ 123.460] myblocking: Try: cat /dev/myblocking (will block until data written)
~~~

### 3. Verify device created

~~~bash
ls -l /dev/myblocking
~~~

## Testing Blocking Behavior

### Experiment 1: Blocking Read

You'll need to use background processes or the job control since we're in a single QEMU terminal.

**Start a blocking read in background:**
~~~bash
cat /dev/myblocking &
~~~

Watch kernel messages:
~~~bash
dmesg | tail
~~~

You'll see:
~~~
[ 234.567] myblocking: Device opened (readers: 1, writers: 0)
[ 234.568] myblocking: Reader going to sleep (no data)
~~~

**Now write data (unblocks reader):**
~~~bash
echo "Hello from writer!" > /dev/myblocking
~~~

The background `cat` wakes up and displays:
~~~
Hello from writer!
[1]+  Done                    cat /dev/myblocking
~~~

Kernel messages show:
~~~
[ 345.678] myblocking: Device opened (readers: 0, writers: 1)
[ 345.679] myblocking: Wrote 19 bytes (read_pos: 0, write_pos: 19)
[ 345.680] myblocking: Waking up readers
[ 345.681] myblocking: Reader woke up!
[ 345.682] myblocking: Read 19 bytes (read_pos: 19, write_pos: 19)
~~~

### Experiment 2: Multiple Readers

Start multiple readers:
~~~bash
cat /dev/myblocking &
cat /dev/myblocking &
~~~

Check processes:
~~~bash
jobs
~~~

Write once:
~~~bash
echo "test" > /dev/myblocking
~~~

Only **one** `cat` will receive the data. The other stays blocked!

To clean up the remaining blocked process:
~~~bash
kill %1  # or kill %2, depending on which is still blocked
~~~

### Experiment 3: Interrupted by Signal

Start a blocking read:
~~~bash
cat /dev/myblocking
# Blocks...
# Press Ctrl+C
^C
~~~

Kernel messages:
~~~
[ 456.789] myblocking: Reader going to sleep (no data)
[ 460.123] myblocking: Reader interrupted by signal
~~~

The `cat` command receives `-ERESTARTSYS` and terminates cleanly.

### Experiment 4: Non-blocking Test with dd

You can also test with `dd`:

~~~bash
# This will block waiting for exactly 10 bytes
dd if=/dev/myblocking of=/tmp/output bs=1 count=10 &

# In the shell, write data
echo "0123456789abcdef" > /dev/myblocking

# dd reads exactly 10 bytes and exits
cat /tmp/output
~~~

## Understanding the Flow

### Read Flow (Blocking)

~~~
1. Process calls read()
        ↓
2. No data available?
        ↓ YES
3. Add process to wait_queue
        ↓
4. Put process to SLEEP
        ↓
5. (waiting...)
        ↓
6. Writer calls wake_up_interruptible()
        ↓
7. Process WAKES UP
        ↓
8. Check condition again (has data?)
        ↓ YES
9. Copy data to userspace
        ↓
10. Return to userspace
~~~

### Write Flow (Unblocking)

~~~
1. Process calls write()
        ↓
2. Copy data from userspace
        ↓
3. Update buffer pointers
        ↓
4. Call wake_up_interruptible()
        ↓
5. All sleeping readers wake up
        ↓
6. Return to userspace
~~~

## Code Highlights

### Wait Queue Declaration and Initialization

~~~C
// In device structure
wait_queue_head_t read_queue;

// In init function
init_waitqueue_head(&my_device->read_queue);
~~~

### Blocking Until Condition

~~~C
// Sleep until has_data(dev) returns true
ret = wait_event_interruptible(dev->read_queue, has_data(dev));

if (ret < 0)
    return -ERESTARTSYS;  // Signal received (Ctrl+C)
~~~

### Waking Up Sleepers

~~~C
// Wake up all processes waiting on read_queue
wake_up_interruptible(&dev->read_queue);
~~~

### Mutex Protection

~~~C
mutex_lock(&dev->lock);
// Critical section - modify shared data
mutex_unlock(&dev->lock);

// Or interruptible version:
if (mutex_lock_interruptible(&dev->lock))
    return -ERESTARTSYS;
~~~

## Important Concepts

### 1. Interruptible vs Uninterruptible Sleep

**Interruptible** (`wait_event_interruptible`):
- Can be woken by signals (Ctrl+C)
- Preferred for user-facing operations
- Returns `-ERESTARTSYS` if interrupted

**Uninterruptible** (`wait_event`):
- Cannot be interrupted by signals
- Shows as "D" state in `ps`
- Use rarely, only when absolutely necessary

### 2. Race Conditions

Always check condition **after** waking up:

~~~C
// CORRECT:
while (!has_data(dev)) {
    wait_event_interruptible(queue, has_data(dev));
}
// Check again before proceeding

// WRONG:
if (!has_data(dev)) {
    wait_event_interruptible(queue, has_data(dev));
    // Data might still not be available!
}
~~~

### 3. Spurious Wakeups

Processes can wake up without `wake_up_interruptible()` being called (spurious wakeups). Always recheck the condition!

### 4. Mutex vs Spinlock

This example uses **mutexes** (`mutex_lock`) for simplicity. Key differences:

| Mutex | Spinlock |
|-------|----------|
| Can sleep while waiting | Busy-waits (burns CPU) |
| Use in process context | Use in interrupt context |
| Can hold for longer | Hold for very short time |
| Can't use in interrupts | Required in interrupts |

For character devices accessed from user processes, mutexes are usually the right choice.

## Monitoring

Watch real-time kernel messages:

~~~bash
dmesg -w
~~~

Then interact with device in the shell to see the blocking/waking behavior.

## Cleanup

~~~bash
rmmod char_dev_blocking
ls /dev/myblocking  # Should be gone
~~~

## Real-World Uses

Blocking I/O with wait queues is used in:

- **Keyboard drivers**: Block until key pressed
- **Serial ports**: Block until data received
- **Pipes/FIFOs**: Block until data available
- **Network sockets**: Block until packet arrives
- **Audio devices**: Block until buffer space available

This is a fundamental pattern in device driver programming!

## Advanced Topics

### poll/select Support (Not in this example)

In a future example, we could add `poll()` support to allow userspace programs to check if data is available without blocking:

~~~C
static unsigned int dev_poll(struct file *filep, poll_table *wait) {
    struct myblock_dev *dev = filep->private_data;
    unsigned int mask = 0;

    poll_wait(filep, &dev->read_queue, wait);

    if (has_data(dev))
        mask |= POLLIN | POLLRDNORM;  // Data available for reading

    return mask;
}
~~~

This would allow programs to use `select()`, `poll()`, or `epoll()` to multiplex I/O on multiple devices.

### O_NONBLOCK Flag (Not implemented)

Real drivers should check the `O_NONBLOCK` flag:

~~~C
if (filep->f_flags & O_NONBLOCK)
    return -EAGAIN;  // Don't block, return immediately
~~~

This allows userspace to choose between blocking and non-blocking I/O:

~~~bash
# Blocking (default)
cat /dev/myblocking

# Non-blocking (with O_NONBLOCK)
# Would return immediately with EAGAIN if no data
~~~

## Troubleshooting

### Reader stays blocked forever?

Make sure you write to the device:
~~~bash
# Check if process is blocked
ps aux | grep cat

# Write data to wake it up
echo "wake up!" > /dev/myblocking
~~~

### Can't remove module - "Device or resource busy"?

Find and kill blocked processes:
~~~bash
# Find PIDs using the device
lsof /dev/myblocking

# Kill them (sends SIGTERM, wakes blocked processes)
kill <PID>

# Or force kill
kill -9 <PID>

# Now you can remove module
rmmod char_dev_blocking
~~~

### Process in "D" state (uninterruptible)?

This shouldn't happen with our driver (we use `_interruptible` versions). If it does:
1. Check kernel logs for errors: `dmesg | tail`
2. Try writing to the device to wake it up
3. Reboot if necessary (last resort)

## Comparison with Previous Examples

### 02-char-device-driver (Basic)
- Read returns immediately with whatever is in buffer
- Empty buffer returns empty data

### 06-char-dev-blocking (This example)
- Read **blocks** until data available
- Models real hardware behavior
- Producer-consumer pattern

## Summary

This example demonstrates:

✅ **Wait queues** - Blocking processes until conditions met
✅ **Producer-consumer** - Writers wake up readers
✅ **Interruptible sleep** - Handles Ctrl+C gracefully
✅ **Mutex protection** - Thread-safe access to shared buffer
✅ **Real blocking behavior** - Like real device drivers!

This is essential knowledge for writing device drivers that interact with real hardware where data isn't immediately available!

## Next Steps

After mastering blocking I/O, you could explore:
- **poll/select/epoll support**: Multiplexing I/O operations
- **Asynchronous I/O**: Using `fasync` for signal-driven I/O
- **mmap support**: Mapping device memory to userspace
- **Block device drivers**: Different I/O model entirely

Now you have a solid foundation in character device drivers! Time to move on to block devices? 🚀
