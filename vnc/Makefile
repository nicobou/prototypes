all: gtkvnc2shm

gtkvnc2shm: gtkvncviewer_shmdata.c 
	gcc gtkvncviewer_shmdata.c -o gtkvnc2shm `pkg-config --cflags --libs libvncclient gtk+-2.0 shmdata-any-0.8`

clean:
	rm gtkvnc2shm
