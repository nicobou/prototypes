all: sharedvideoreader sharedvideowriter clutterdemo 

sharedvideoreader: shared-video-reader.c
	gcc -Wall shared-video-reader.c -o shared-video-reader `pkg-config --cflags --libs gstreamer-0.10 gio-2.0`

sharedvideowriter: shared-video-writer.c
	gcc -Wall shared-video-writer.c -o shared-video-writer `pkg-config --cflags --libs gstreamer-0.10`

clutterdemo: cluttershare.c
	g++ -Wall -fpermissive -g cluttershare.c -o clutterdemo  `pkg-config --cflags --libs gstreamer-0.10 clutter-1.0 glew` -lgstgl-0.10

clean:
	rm -f shared-video-reader shared-video-writer clutterdemo *.o

