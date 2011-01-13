CC     = gcc

#For the camera API
CVER = 4.4
CPU = x86
PVAPI_INC_DIR = camera_sdk/inc-pc
PVAPI_BIN_DIR = camera_sdk/bin-pc/$(CPU)
PVAPI_LIB_DIR = camera_sdk/lib-pc/$(CPU)


CFLAGS = `pkg-config --cflags gtk+-2.0 gmodule-2.0` -DMAGICKCORE_EXCLUDE_DEPRECATED -D_LINUX -D_$(CPU)  -I$(PVAPI_INC_DIR) `MagickWand-config --cflags`
LIBS   = `pkg-config --libs   gtk+-2.0 gmodule-2.0` -lpthread -lrt -L$(PVAPI_BIN_DIR) -lPvAPI `MagickWand-config --libs`
DEBUG  = -Wall -g
OPTS   = -O2

OBJECTS = camera_interface.o camera_control.o camera_imagemagick.o

.PHONY: clean

all: camera_interface

camera_interface: $(OBJECTS)
	$(CC) $(OPTS) $(DEBUG) $(LIBS) $(OBJECTS) -o $@

camera_interface.o: camera_interface.c camera.h
	$(CC) $(OPTS) $(DEBUG) $(CFLAGS) -c $< -o $@

camera_imagemagick.o: camera_imagemagick.c camera.h
	$(CC) $(OPTS) $(DEBUG) $(CFLAGS) -c $< -o $@

camera_control.o: camera_control.c camera.h
	$(CC) $(OPTS) $(DEBUG) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o camera_interface

