# linuxKernel

git clone https://github.com/ssukijth0330/linuxKernel.git
cd linuxKerneel/linux-6.12.1

#Ensure the built environment is previously setup.

make menuconfig    save the .config
make testconfig
make -j16       : -j is optional, 16 is the number of core if system has 16 cores.
---------------
make clean
