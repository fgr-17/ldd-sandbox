### 1. hello-world
Basic kernel module demonstrating:
- Module initialization and cleanup
- Kernel logging with `printk()`
- Module parameters
- Loading/unloading modules

### Loading module in QEMU

1. Compile the module in the docker container `ldd-sandbox`
2. Once compiled, the module should be ready in `/workspace/modules/01-hello-world/hello.ko`
3. When running `./run-qemu.sh` inside the container, the script should ship all the `*.ko` files inside `initramfs`, exposing them in `/modules/*.ko`
4. Install the module running `insmod /modules/hello.ko`
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
cp /modules/hello.ko /lib/modules/$(uname -r)/
~~~

3. Generate the modules.dep file

~~~bash
depmod -a
~~~

4. Now `modinfo` should work

~~~bash
modinfo hello
~~~
