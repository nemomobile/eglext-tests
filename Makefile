CFLAGS=-DSUPPORT_X11 -Wall -ggdb
CXXFLAGS=-DSUPPORT_X11 -Wall -ggdb
LDFLAGS=-lX11 -lGLESv2 -lEGL -lrt `pkg-config --libs xcomposite`

OBJS=src/native_x11.o src/util.o src/testutil.o

all: src/test_image \
    src/test_shared_image \
    src/test_swap_region \
    src/test_lock_surface \
    src/test_scaling \
    src/test_fence_sync

src/test_image: $(OBJS)

src/test_shared_image: $(OBJS)

src/test_swap_region: $(OBJS)

src/test_lock_surface: $(OBJS)

src/test_scaling: $(OBJS)

src/test_fence_sync: $(OBJS)

install: all
	mkdir -p $(DESTDIR)/usr/bin
	install \
	    src/test_image \
	    src/test_shared_image \
	    src/test_swap_region \
	    src/test_lock_surface \
	    src/test_scaling \
	    src/test_fence_sync \
	    $(DESTDIR)/usr/bin
	mkdir -p $(DESTDIR)/usr/share/eglext-tests
	install data/*.raw $(DESTDIR)/usr/share/eglext-tests

clean:
	rm -f src/*.o \
	    src/test_image \
	    src/test_shared_image \
	    src/test_swap_region \
	    src/test_lock_surface \
	    src/test_scaling \
	    src/test_fence_sync
