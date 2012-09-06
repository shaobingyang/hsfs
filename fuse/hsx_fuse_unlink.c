/**
 * hsx_fuse_unlink.c
 * yanhuan
 * 2012.9.5
 **/

#include "../include/hsx_fuse.h"

#define MAXNAMELEN 255

extern void hsx_fuse_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	int err = 0;
	if (name == NULL) {
		ERR("%s name to remove is NULL\n", progname);
		err = ENOENT;
		goto out;
	}
	else if (!(strcmp(name, ""))) {
		ERR("%s name to remove is empty\n", progname);
		err = ENOENT;
		goto out;
	}
	else if (strlen(name) > MAXNAMELEN) {
		ERR("%s name to remove is too long\n", progname);
		err = ENAMETOOLONG;
		goto out;
	}

	struct hsfs_super *sb = NULL;
	struct hsfs_inode *hi = NULL;
	if (!(hs = fuse_req_userdata(req))) {
		ERR("%s get user data failed\n", progname);
		err = EIO;
		goto out;
	}
	if (!(hi = hsx_fuse_iget(sb, parent))) {
		ERR("%s get hsfs inode failed\n", progname);
		err = EIO;
		goto out;
	}

	if ((err = hsi_nfs3_remove(hi, name))) {
		goto out;
	}
out:
	/* remove ino <=> fh? */
	fuse_reply_err(req, err);
}
