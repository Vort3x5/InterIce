sudo sh -c "echo 0x400000 > /sys/kernel/sykt_sysfs/dsskma"
sudo sh -c "echo 1 > /sys/kernel/sykt_sysfs/dtskma"
cat /sys/kernel/sykt_sysfs/drskma
