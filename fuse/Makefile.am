sbindir = /sbin

noinst_PROGRAMS = hsfs
hsfs_CPPFLAGS = $(AM_CFLAGS) -g -Wall -I/usr/include/tirpc/ \
			`pkg-config fuse --cflags` 
hsfs_LDFLAGS = `pkg-config fuse --libs`
hsfs_LDADD = $(top_builddir)/fuse/libhsx_fuse.a \
		     $(top_builddir)/nfs3/libhsi_nfs3.a
hsfs_SOURCES = xcommon.c hsfs_main.c

noinst_LIBRARIES = libhsx_fuse.a
libhsx_fuse_a_CPPFLAGS = $(AM_CFLAGS) -g -Wall -I/usr/include/tirpc/ \
			`pkg-config fuse --cflags`
libhsx_fuse_a_SOURCES = hsfs_fuse_table.c hsx_fuse_getattr.c hsx_fuse_statfs.c \
			hsx_fuse_lookup.c hsx_fuse_fill_reply.c hsx_fuse_mkdir.c \
			hsx_fuse_read.c hsx_fuse_write.c hsx_fuse_open.c hsx_fuse_release.c \
			hsx_fuse_rmdir.c hsx_fuse_forget.c hsx_fuse_setattr.c \
			hsx_fuse_readlink.c hsx_fuse_symlink.c \
			hsx_fuse_unlink.c hsx_fuse_rename.c hsx_fuse_readdir.c \
			hsx_fuse_opendir.c hsx_fuse_setxattr.c \
			hsx_fuse_mknod.c hsx_fuse_link.c hsx_fuse_create.c \
			hsx_fuse_access.c hsx_fuse_getxattr.c

MOUNT_HSFS = mount.hsfs
install:
	@if test -e $(sbindir)/$(MOUNT_HSFS); then \
		echo "mount.hsfs has exist, delete first.";\
		rm $(sbindir)/$(MOUNT_HSFS) -f ;\
	fi;
	cp hsfs $(sbindir)/$(MOUNT_HSFS) -f
uninstall:
	rm $(sbindir)/$(MOUNT_HSFS) -f
