### Steps to run Character driver - custom_cdev

sudo insmod hello_world.ko
echo 42 | sudo tee /sys/module/hello_world/parameters/value_cb
dmesg | tail