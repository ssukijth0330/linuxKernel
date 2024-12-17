# linuxKernel

git clone https://github.com/ssukijth0330/linuxKernel.git
cd linuxKerneel/linux-6.12.1
make menuconfig    save the .config
make testconfig
make -j[[number of core]    : -j is optional
---------------
make clean
