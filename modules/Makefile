KVERS = ${shell uname -r}

obj-m += do_execve.o

build : kernel_modules
kernel_modules : 
	make -C /lib/modules/${KVERS}/build  M=${CURDIR} modules

cleanall:
	rm -rf *.ko
clean : 
	rm -rf *.o
	rm -rf modules.order 
	rm -rf Module.symvers
	rm -rf do_execve.mod*
	rm -rf .do_execve*
	rm -rf .tmp_versions
