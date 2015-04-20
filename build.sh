#! /usr/bin/env zsh
set -e ; set -u
make -j5 nativekernel KERNCONF=VMWARE $@
make installkernel KERNCONF=VMWARE $@
