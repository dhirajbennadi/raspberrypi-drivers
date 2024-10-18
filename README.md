# raspberrypi-drivers
Implementation of custom drivers on Raspberry Pi

# Steps to run
1. Compile each of the driver and user source files by running make
2. Create a character node - sudo mknod /dev/$(node_name) c 42 0
3. Insert the kernel object - sudo insmod custom_cdev.ko
4. Provide permissions for read and write - sudo chmod 666 /dev/custom_cdev
5. Execute the user code from user directory
