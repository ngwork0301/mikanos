#!/bin/bash -xe
BASEDIR="$HOME/osbook/devenv/x86_64-elf"
EDK2DIR="$HOME/edk2"

if [ ! -d $BASEDIR ]
then
    echo "$BASEDIR が存在しません。"
    exit 1
else
    export CPPFLAGS="\
    -I$BASEDIR/include/c++/v1 -I$BASEDIR/include -I$BASEDIR/include/freetype2 \
    -I$EDK2DIR/MdePkg/Include -I$EDK2DIR/MdePkg/Include/X64 \
    -nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS \
    -DEFIAPI='__attribute__((ms_abi))'"
    export LDFLAGS="-L$BASEDIR/lib"
fi

clang++ -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone \
  -fno-exceptions -fno-rtti -std=c++17 ${CPPFLAGS} -c main.cpp
ld.lld --entry KernelMain -z norelro --image-base 0x100000 --static \
  ${LDFLAGS} -o kernel.elf main.o
