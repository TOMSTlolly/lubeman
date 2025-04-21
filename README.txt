+) zaloz 
/etc/udev/rules.d/99-ftdi.rules

+) sudo dmesg |grep FTDI
+) zjisti vid/pid a prepis ftdi.rules
+) sudo udevadm control --reload-rules
+) sudo udevadm trigger

*********** vnutim x86 ubuntu knihovny BEZ behu ve virtualnim prostredi
+) instaluj tzlocal
   sudo apt install python3-tzlocal

+) instaluj ipc komunikaci
   sudo apt install python3-sysv-ipc


