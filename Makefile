all:
	gcc -g -Wall xyaffs2.c -o xyaffs2
	gcc -g -DWITH_BRN_BBF -Wall xyaffs2.c -o brn_xyaffs2
clean:
	rm -f xyaffs2
