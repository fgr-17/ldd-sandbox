# 07-char-dev-poll - poll/select Support

This example demonstrates `poll()`, `select()`, and `epoll()` support for efficient I/O multiplexing without blocking.

## What's New: I/O Multiplexing

### Previous Example (Blocking)
~~~bash
# This blocks until data available
cat /dev/myblocking
~~~

### This Example (Non-blocking Check)
~~~C
// Check if device is ready WITHOUT blocking
struct pollfd pfd = {.fd = fd, .events = POLLIN};
poll(&pfd, 1, 0);  // Returns immediately

if (pfd.revents & POLLIN)
    read(fd, buffer, size);  // Won't block - data available!
~~~

## Key Concepts

### What is poll/select?

**poll()** and **select()** allow a program to monitor multiple file descriptors to see if I/O is possible on any of them without blocking.

**Use cases:**
- Web servers handling many connections
- Programs reading from multiple devices
- GUI applications that need to stay responsive

### The poll() File Operation

~~~C
static __poll_t dev_poll(struct file *filep, poll_table *wait) {
    __poll_t mask = 0;

    // Register wait queues (doesn't block!)
    poll_wait(filep, &dev->read_queue, wait);
    poll_wait(filep, &dev->write_queue, wait);

    // Check current state
    if (has_data(dev))
        mask |= POLLIN | POLLRDNORM;   // Readable

    if (has_space(dev))
        mask |= POLLOUT | POLLWRNORM;  // Writable

    return mask;
}
~~~

### Poll Event Flags

| Flag | Meaning |
|------|---------|
| `POLLIN` | Data available for reading |
| `POLLOUT` | Space available for writing |
| `POLLRDNORM` | Normal data may be read |
| `POLLWRNORM` | Normal data may be written |
| `POLLERR` | Error condition |
| `POLLHUP` | Hang up |
| `POLLNVAL` | Invalid request |

## Building and Testing

### 1. Build the module and test program

~~~bash
cd 07-char-dev-poll
make          # Builds kernel module
make test     # Builds userspace test program
~~~

### 2. Load in QEMU

~~~bash
./run-qemu.sh
insmod /modules/char-dev-poll.ko
dmesg | tail
~~~

Output:
~~~
[ 123.456] mypoll: Initializing module...
[ 123.457] mypoll: Allocated major number: 240
[ 123.458] mypoll: Module loaded successfully!
[ 123.459] mypoll: Device /dev/mypoll created
~~~

### 3. Verify device created

~~~bash
ls -l /dev/mypoll
~~~

### 4. Run the test program

The test program should be bundled in initramfs at `/usr/bin/test_poll`:

~~~bash
/usr/bin/test_poll
~~~

Expected output:
~~~
Character Device poll/select Test Program
==========================================

=== Testing select() ===

1. Checking if device is readable (empty buffer)...
   Timeout: Device is NOT readable (expected)

2. Checking if device is writable (has space)...
   Device is WRITABLE (expected)

3. Writing data to device...
   Data written

4. Checking if device is now readable...
   Device is READABLE (expected)
   Read: Hello poll!

=== select() tests complete ===

=== Testing poll() ===

1. Polling for POLLIN (empty buffer)...
   Timeout: No events (expected)

2. Polling for POLLOUT (has space)...
   POLLOUT: Device is writable (expected)

3. Writing data...

4. Polling for POLLIN (after write)...
   POLLIN: Device is readable (expected)
   Read: Hello from poll!

5. Polling for POLLIN | POLLOUT...
   Events returned:
     - POLLOUT (writable)

=== poll() tests complete ===

=== Testing Multiple File Descriptors ===

1. Writing to first fd...

2. Polling both fds for readability...
   fd1 is readable
   fd2 is readable (same device!)

=== Multiple FD tests complete ===

All tests complete!
~~~

## Manual Testing

### Test 1: Check Readability (Empty Buffer)

~~~bash
# Create a simple test using timeout
timeout 2 cat /dev/mypoll
~~~

This should timeout because there's no data.

### Test 2: Write Then Read

~~~bash
# Write some data
echo "test data" > /dev/mypoll

# Now read (should return immediately)
timeout 2 cat /dev/mypoll
~~~

Should display "test data" immediately.

### Test 3: Monitor with dmesg

~~~bash
# In one terminal
dmesg -w

# In another terminal (or background)
/modules/test_poll
~~~

Watch the kernel messages showing poll() calls and state checks.

## Understanding poll_wait()

### What poll_wait() Does

~~~C
poll_wait(filep, &dev->read_queue, wait);
~~~

This **does NOT block**! It only:
1. Registers the wait queue with the poll table
2. Allows the kernel to wake the polling process when events occur

### The Flow

~~~
1. Userspace calls poll()
        ↓
2. Kernel calls dev_poll()
        ↓
3. dev_poll() calls poll_wait() for each queue
   (registers interest)
        ↓
4. dev_poll() checks current state
        ↓
5. Returns event mask to kernel
        ↓
6. If events ready → return to userspace immediately
   If no events → kernel sleeps process
        ↓
7. When wake_up_interruptible() called:
   → Kernel re-calls dev_poll()
   → Checks state again
   → Returns to userspace if ready
~~~

## Code Highlights

### The poll() Operation

~~~C
static __poll_t dev_poll(struct file *filep, poll_table *wait) {
    struct mypoll_dev *dev = filep->private_data;
    __poll_t mask = 0;

    // Register our wait queues
    poll_wait(filep, &dev->read_queue, wait);
    poll_wait(filep, &dev->write_queue, wait);

    mutex_lock(&dev->lock);

    // Check readable
    if (has_data(dev))
        mask |= POLLIN | POLLRDNORM;

    // Check writable
    if (has_space(dev))
        mask |= POLLOUT | POLLWRNORM;

    mutex_unlock(&dev->lock);

    return mask;
}
~~~

### Using poll() in Userspace

~~~C
#include <poll.h>

struct pollfd pfd;
pfd.fd = open("/dev/mypoll", O_RDWR);
pfd.events = POLLIN | POLLOUT;  // Monitor read and write

int ret = poll(&pfd, 1, 5000);  // 5 second timeout

if (ret > 0) {
    if (pfd.revents & POLLIN)
        read(pfd.fd, buffer, size);  // Won't block

    if (pfd.revents & POLLOUT)
        write(pfd.fd, data, size);   // Won't block
}
~~~

### Using select() in Userspace

~~~C
#include <sys/select.h>

fd_set readfds;
struct timeval timeout;
int fd = open("/dev/mypoll", O_RDWR);

FD_ZERO(&readfds);
FD_SET(fd, &readfds);
timeout.tv_sec = 5;
timeout.tv_usec = 0;

int ret = select(fd + 1, &readfds, NULL, NULL, &timeout);

if (ret > 0 && FD_ISSET(fd, &readfds))
    read(fd, buffer, size);  // Won't block
~~~

## Advantages of poll/select

| Blocking Read | poll/select |
|---------------|-------------|
| Blocks on one device | Monitor multiple devices |
| Must use threads for multiple I/O | Single thread handles many I/O |
| Wastes CPU with busy-wait | Efficient: kernel handles waiting |
| Can't timeout easily | Built-in timeout support |

## Real-World Examples

### Web Server Pattern

~~~C
struct pollfd fds[MAX_CLIENTS];

while (1) {
    // Monitor all client connections
    int ready = poll(fds, num_clients, -1);

    for (int i = 0; i < num_clients; i++) {
        if (fds[i].revents & POLLIN) {
            // This client has data ready
            handle_client(fds[i].fd);
        }
    }
}
~~~

### Multi-device Reader

~~~C
struct pollfd fds[3];
fds[0].fd = open("/dev/mypoll", O_RDONLY);
fds[1].fd = open("/dev/input/event0", O_RDONLY);
fds[2].fd = open("/dev/ttyS0", O_RDONLY);

for (int i = 0; i < 3; i++)
    fds[i].events = POLLIN;

while (1) {
    poll(fds, 3, -1);

    if (fds[0].revents & POLLIN)
        handle_mypoll();
    if (fds[1].revents & POLLIN)
        handle_input();
    if (fds[2].revents & POLLIN)
        handle_serial();
}
~~~

## Monitoring

Watch kernel messages while running tests:

~~~bash
dmesg -w
~~~

You'll see:
~~~
[ 234.567] mypoll: poll() called
[ 234.568] mypoll: Device is WRITABLE
[ 345.678] mypoll: Wrote 10 bytes (read_pos: 0, write_pos: 10)
[ 345.679] mypoll: poll() called
[ 345.680] mypoll: Device is READABLE
[ 345.681] mypoll: Device is WRITABLE
~~~

## Cleanup

~~~bash
rmmod char_dev_poll
ls /dev/mypoll  # Should be gone
~~~

## Comparison with Previous Examples

| Example | Behavior |
|---------|----------|
| 02-char-device-driver | Returns immediately, even if no data |
| 06-char-dev-blocking | Blocks until data available |
| 07-char-dev-poll | Check without blocking, wait only if needed |

## Advanced: epoll Support

The same `poll()` operation supports `epoll()` - Linux's scalable I/O event notification:

~~~C
int epfd = epoll_create1(0);
struct epoll_event ev;

ev.events = EPOLLIN;
ev.data.fd = fd;
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

struct epoll_event events[10];
int nfds = epoll_wait(epfd, events, 10, 5000);

for (int i = 0; i < nfds; i++) {
    if (events[i].events & EPOLLIN)
        read(events[i].data.fd, buffer, size);
}
~~~

Your driver's `poll()` operation automatically works with `epoll()`!

## O_NONBLOCK Support

This example implements `O_NONBLOCK` checking in the read operation:

~~~C
// In dev_read()
if ((filep->f_flags & O_NONBLOCK) && !has_data(dev)) {
    return -EAGAIN;  // Return immediately, don't block
}
~~~

Test it:

~~~bash
# This will return immediately with EAGAIN if no data
cat <&3 3</dev/mypoll &  # Background, non-blocking open

# Or use a test program that opens with O_NONBLOCK
~~~

## Troubleshooting

### poll() always returns immediately?

Check that you're:
1. Calling `poll_wait()` for the relevant queues
2. Returning 0 when no events are ready
3. Calling `wake_up_interruptible()` when state changes

### poll() never returns?

Make sure you:
1. Return the appropriate event mask when ready
2. Call `wake_up_interruptible()` after state changes
3. Check both read and write conditions

### Test program not found?

The test program needs to be bundled in initramfs. Check `run-qemu.sh` bundles the test directory, or manually compile and copy it in QEMU:

~~~bash
# Inside QEMU
gcc -o /tmp/test_poll /modules/test_poll.c
/tmp/test_poll
~~~

### "Device busy" when removing module?

Make sure no processes have the device open:

~~~bash
lsof /dev/mypoll
# Kill any processes using it
killall test_poll
# Now remove
rmmod char_dev_poll
~~~

## How poll() Differs from Blocking I/O

### Blocking I/O (Previous Example)
~~~C
read(fd, buffer, size);  // Blocks here until data available
~~~

### poll() then read (This Example)
~~~C
poll(&pfd, 1, timeout);   // Wait for readability
if (pfd.revents & POLLIN)
    read(fd, buffer, size);  // Won't block - we know data is ready
~~~

The key difference: **poll() tells you WHEN to read/write**, so you never waste time blocking on unready devices.

## Performance Benefits

### Without poll() - Multiple Devices with Threads
~~~
Thread 1: read(dev1)  ← Blocked
Thread 2: read(dev2)  ← Blocked
Thread 3: read(dev3)  ← Blocked
Cost: 3 threads, 3 stacks, context switching
~~~

### With poll() - Single Thread
~~~
poll([dev1, dev2, dev3])  ← Efficient wait
Only read from ready devices
Cost: 1 thread, no wasted blocking
~~~
