all:
	gcc -Wall passthrough.c `pkg-config fuse3 --cflags --libs` -g -o passthrough
