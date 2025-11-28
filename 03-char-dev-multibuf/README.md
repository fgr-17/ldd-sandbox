### 3. char-dev-multibuf
Basic character device driver demonstrating:
- Character device registration with `register_chrdev()`
- File operations structure (`file_operations`)
- Read and write operations between user space and kernel space
- Using `copy_to_user()` and `copy_from_user()` for safe data transfer
- Device node creation and interaction

### Loading module in QEMU

1. Compile the module in the docker container `ldd-sandbox`
2. Once compiled, the module should be ready in `/workspace/modules/02-char-device-driver/char-device-driver.ko`
3. When running `./run-qemu.sh` inside the container, the script should ship all the `*.ko` files inside `initramfs`, exposing them in `/modules/*.ko`
4. Install the module running `insmod /modules/char-device-driver.ko`
5. Check the module messages running `dmesg | tail`
6. Remove the module running `rmmod hello`

### Getting module info

To get the module info as described in the [hello.c](./src/hello.c):

~~~C
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Hello World module");
MODULE_VERSION("1.0");
~~~

you need to follow a couple of steps. This is usually done with `modinfo` tool, but it source of info is completely disconnected from `insmod`, so it needs to be generated individually. So, follow this steps to create the database so `modinfo` can bring the data you need:

1. Create the directory structure

~~~bash
mkdir -p /lib/modules/$(uname -r)
~~~

2. Copy your module there

~~~bash
cp /modules/char-device-driver.ko /lib/modules/$(uname -r)/
~~~

3. Generate the modules.dep file

~~~bash
depmod -a
~~~

4. Now `modinfo` should work

~~~bash
modinfo char-device-driver
~~~

### Reeading and writing from nodes

Once that the `char-device-driver` is installed, what you need is at least one or more node, that will act as a bridge between the linux user and kernel space. The node is created passing a **major number** that specifies which driver handles the node. THen the kernel uses this to route operations to your driver's `file_operations` functions.

This **major number** is exposed when installing the module with `insmod`. As an example:

~~~bash
/ # insmod /modules/char-device-driver.ko
[ 2337.307132] mychardev: Registered with major number 241
[ 2337.308730] Create device: mknod /dev/mychardev c 241 0
~~~

So, as the message clearly says, you can create a node to connect user and kernel space by executing:

~~~bash
mknod /dev/mychardev c 241 0
~~~

Then you can read and write to that file as usual, for example:

**Writing:**
~~~bash
/ # echo "hi from user space" > /dev/mychardev
[ 2458.431564] mychardev: Device opened
[ 2458.436521] mychardev: Received 19 bytes from user
[ 2458.443186] mychardev: Device closed
~~~

**Reading:**

~~~bash
cat /dev/mychardev
[ 2498.114663] mychardev: Device opened
[ 2498.119845] mychardev: Sent 19 bytes to user
hi from user space
[ 2498.128736] mychardev: Device closed
~~~

You can create multiple nodes changing the **device path** and **minor number**:

~~~bash
mknod /dev/mychardev0 c 241 0
mknod /dev/mychardev1 c 241 1
~~~

In this example, each device should manage its own buffer, so you should be able to write/read on each device separately
