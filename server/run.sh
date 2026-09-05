#!/usr/bin/bash
#注释

make clean

make

if [ -f "server" ]
then
# ulimit -n 100000
	./server
else
	echo "没有生成可执行程序server!"
fi

