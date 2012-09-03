
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mntent.h>
#include <errno.h>

#include "log.h"
#include "hsfs.h"
#include "conn.h"
#include "nfs3.h"
#include "mount.h"
#include "xcommon.h"
#include "fstab.h"

typedef dirpath mntarg_t;
typedef dirpath umntarg_t;
typedef struct mountres3 mntres_t;

#define NFS_MOUNT_TCP		0x0001

static int hsi_gethostbyname(const char *hostname, struct sockaddr_in *saddr)
{
	struct hostent *hp = NULL;

	saddr->sin_family = AF_INET;
	if (!inet_aton(hostname, &saddr->sin_addr)) {
		if ((hp = gethostbyname(hostname)) == NULL) {
			ERR("mount: can't get address for %s.", hostname);
			return errno;
		} else {
			if (hp->h_length > sizeof(*saddr)) {
				ERR("mount: got bad hp->h_length.");
				hp->h_length = sizeof(*saddr);
			}
			memcpy(&saddr->sin_addr, hp->h_addr, hp->h_length);
		}
	}
	return 0;
}

static int hsi_nfsmnt_check_compat(const struct pmap *nfs_pmap,
				const struct pmap *mnt_pmap)
{
	if (nfs_pmap->pm_vers) {
		if (nfs_pmap->pm_vers != 3) {
			ERR("%s: NFS version %ld is not supported, only 3.",
					progname, nfs_pmap->pm_vers);
			goto out_bad;
		}
	}

	if (mnt_pmap->pm_vers) {
		if (mnt_pmap->pm_vers != 3)
			ERR("%s: NFS mount version %ld is not supported, only 3.",
				progname, mnt_pmap->pm_vers);
		goto out_bad;
	}

	return 0;
out_bad:
	return 1;
}

static int hsi_nfs3_mount(clnt_addr_t *mnt_server,
				mntarg_t *mntarg, mntres_t *mntres)
{
	return 0;
}

static int hsi_nfs3_unmount(clnt_addr_t *mnt_server, umntarg_t *umntarg)
{
	return 0;
}

static CLIENT *hsi_nfs3_clnt_create(clnt_addr_t *nfs_server)
{
	return NULL;
}

static int hsi_add_mtab(const char *spec, const char *mount_point,
			char *fstype, int flags, char *opts)
{
	return 0;
}

static int hsi_nfs3_parse_options(char *old_opts, struct hsfs_super *super,
				  int *retry, clnt_addr_t *mnt_server,
				  clnt_addr_t *nfs_server,
				  char *new_opts, const int opt_size)
{
	struct sockaddr_in *mnt_saddr = &mnt_server->saddr;
	struct pmap *mnt_pmap = &mnt_server->pmap;
	struct pmap *nfs_pmap = &nfs_server->pmap;
	char *opt, *opteq, *p, *opt_b, *tmp_opts;
	int open_quote = 0, len = 0;
	char *mounthost = NULL;
	char cbuf[128];

	len = strlen(new_opts);
	tmp_opts = xstrdup(old_opts);
	for (p=tmp_opts, opt_b=NULL; p && *p; p++) {
		if (!opt_b)
			opt_b = p;		/* begin of the option item */
		if (*p == '"')
			open_quote ^= 1;	/* reverse the status */
		if (open_quote)
			continue;		/* still in a quoted block */
		if (*p == ',')
			*p = '\0';		/* terminate the option item */
		if (*p == '\0' || *(p+1) == '\0') {
			opt = opt_b;		/* opt is useful now */
			opt_b = NULL;
		}
		else
			continue;		/* still somewhere in the option item */

		if (strlen(opt) >= sizeof(cbuf))
			goto bad_parameter;
		if ((opteq = strchr(opt, '=')) && isdigit(opteq[1])) {
			int val = atoi(opteq + 1);	
			*opteq = '\0';
			if (!strcmp(opt, "rsize"))
				super->rsize = val;
			else if (!strcmp(opt, "wsize"))
				super->wsize = val;
			else if (!strcmp(opt, "timeo"))
				super->timeo = val;
			else if (!strcmp(opt, "retrans"))
				super->retrans = val;
			else if (!strcmp(opt, "acregmin"))
				super->acregmin = val;
			else if (!strcmp(opt, "acregmax"))
				super->acregmax = val;
			else if (!strcmp(opt, "acdirmin"))
				super->acdirmin = val;
			else if (!strcmp(opt, "acdirmax"))
				super->acdirmax = val;
			else if (!strcmp(opt, "actimeo")) {
				super->acregmin = val;
				super->acregmax = val;
				super->acdirmin = val;
				super->acdirmax = val;
			}
			else if (!strcmp(opt, "retry"))
				*retry = val;
			else if (!strcmp(opt, "port"))
				nfs_pmap->pm_port = val;
			else if (!strcmp(opt, "mountport"))
			        mnt_pmap->pm_port = val;
			else if (!strcmp(opt, "mountprog"))
				mnt_pmap->pm_prog = val;
			else if (!strcmp(opt, "mountvers"))
				mnt_pmap->pm_vers = val;
			else if (!strcmp(opt, "mounthost"))
				mounthost=xstrndup(opteq+1, strcspn(opteq+1," \t\n\r,"));
			else if (!strcmp(opt, "nfsprog"))
				nfs_pmap->pm_prog = val;
			else if (!strcmp(opt, "nfsvers") ||
				 !strcmp(opt, "vers")) {
				nfs_pmap->pm_vers = val;
				opt = "nfsvers";
			} else if (!strcmp(opt, "namlen")) {
				continue;
			} else if (!strcmp(opt, "addr")) {
				/* ignore */;
				continue;
 			} else
				goto bad_parameter;
			sprintf(cbuf, "%s=%s,", opt, opteq+1);
		} else if (opteq) {
			*opteq = '\0';
			if (!strcmp(opt, "proto")) {
				if (!strcmp(opteq+1, "udp")) {
					nfs_pmap->pm_prot = IPPROTO_UDP;
					mnt_pmap->pm_prot = IPPROTO_UDP;
					super->flags &= ~NFS_MOUNT_TCP;
				} else if (!strcmp(opteq+1, "tcp")) {
					nfs_pmap->pm_prot = IPPROTO_TCP;
					mnt_pmap->pm_prot = IPPROTO_TCP;
					super->flags |= NFS_MOUNT_TCP;
				} else
					goto bad_parameter;
			} else if (!strcmp(opt, "sec")) {
				char *secflavor = opteq+1;
				WARNING("Warning: ignoring sec=%s option", secflavor);
					continue;
			} else if (!strcmp(opt, "mounthost"))
				mounthost=xstrndup(opteq+1,
						   strcspn(opteq+1," \t\n\r,"));
			 else if (!strcmp(opt, "context")) {
				WARNING("Warning: ignoring context option");
			 } else
				goto bad_parameter;
			sprintf(cbuf, "%s=%s,", opt, opteq+1);
		} else {
			int val = 1;
			if (!strncmp(opt, "no", 2)) {
				val = 0;
				opt += 2;
			}
			if (!strcmp(opt, "bg"))
				continue;
			else if (!strcmp(opt, "fg"))
				continue;
			else if (!strcmp(opt, "soft")) {
				continue;
			} else if (!strcmp(opt, "hard")) {
				continue;
			} else if (!strcmp(opt, "intr")) {
				continue;
			} else if (!strcmp(opt, "posix")) {
				continue;
			} else if (!strcmp(opt, "cto")) {
				continue;
			} else if (!strcmp(opt, "ac")) {
				continue;
			} else if (!strcmp(opt, "tcp")) {
				if (val) {
					nfs_pmap->pm_prot = IPPROTO_TCP;
					mnt_pmap->pm_prot = IPPROTO_TCP;
					super->flags |= NFS_MOUNT_TCP;
				} else {
					mnt_pmap->pm_prot = IPPROTO_UDP;
					nfs_pmap->pm_prot = IPPROTO_UDP;
				}
			} else if (!strcmp(opt, "udp")) {
				super->flags &= ~NFS_MOUNT_TCP;
				if (!val) {
					nfs_pmap->pm_prot = IPPROTO_TCP;
					mnt_pmap->pm_prot = IPPROTO_TCP;
					super->flags |= NFS_MOUNT_TCP;
				} else {
					nfs_pmap->pm_prot = IPPROTO_UDP;
					mnt_pmap->pm_prot = IPPROTO_UDP;
				}
			} else if (!strcmp(opt, "lock")) {
				continue;
			} else if (!strcmp(opt, "broken_suid")) {
				continue;
			} else if (!strcmp(opt, "acl")) {
				continue;
			} else if (!strcmp(opt, "rdirplus")) {
				continue;
			} else if (!strcmp(opt, "sharecache")) {
				continue;
			} else {
				WARNING("%s: Unsupported nfs mount option:"
						" %s%s", progname,
						val ? "" : "no", opt);
				goto out_bad;
			}
			sprintf(cbuf, val ? "%s," : "no%s,", opt);
		}
		len += strlen(cbuf);
		if (len >= opt_size) {
			ERR("%s: excessively long option argument", progname);
			goto out_bad;
		}
		strcat(new_opts, cbuf);
	}
	/* See if the nfs host = mount host. */
	if (mounthost) {
		if (hsi_gethostbyname(mounthost, mnt_saddr) != 0)
			goto out_bad;
		*mnt_server->hostname = mounthost;
	}
	free(tmp_opts);
	return 0;
 bad_parameter:
	ERR("%s: Bad nfs mount parameter: %s\n", progname, opt);
	return 0;
 out_bad:
	free(tmp_opts);
	return 1;
}

struct fuse_chan *hsi_fuse_mount(const char *spec, const char *point,
				  struct fuse_args *args, char *udata,
				  struct hsfs_super *super)
{
	struct fuse_chan *ch = NULL;
	char hostdir[1024] = {};
	char *hostname, *dirname, *old_opts, *mounthost = NULL;
	char new_opts[1024] = {};
	clnt_addr_t mnt_server = { 
		.hostname = &mounthost 
	};
	clnt_addr_t nfs_server = { 
		.hostname = &hostname 
	};

	struct sockaddr_in *nfs_saddr = &nfs_server.saddr;
	struct pmap  *mnt_pmap = &mnt_server.pmap,
		     *nfs_pmap = &nfs_server.pmap;
	struct pmap  save_mnt, save_nfs;

	mntres_t mntres = {};
	
	int retry = 0, ret = 0;
	char *s = NULL;
	time_t timeout, t;

	if (strlen(spec) >= sizeof(hostdir)) {
		ERR("%s: excessively long host:dir argument.", progname);
		goto fail;
	}

	strcpy(hostdir, spec);
	if ((s = strchr(hostdir, ':'))) {
		hostname = hostdir;
		dirname = s + 1;
		*s = '\0';
		/* Ignore all but first hostname in replicated mounts
		   until they can be fully supported. (mack@sgi.com) */
		if ((s = strchr(hostdir, ','))) {
			*s = '\0';
			ERR("%s: warning: multiple hostnames not supported.",
				progname);
		}
	} else {
		ERR("%s: directory to mount not in host:dir format.", progname);
		goto fail;
	}

	if (hsi_gethostbyname(hostname, nfs_saddr) != 0)
		goto fail;

	mounthost = hostname;
	memcpy (&mnt_server.saddr, nfs_saddr, sizeof (mnt_server.saddr));

	/* add IP address to mtab options for use when unmounting */
	s = inet_ntoa(nfs_saddr->sin_addr);

	old_opts = udata;
	if (!old_opts)
		old_opts = "";

	/* Set default options. */
	super->acregmin	= 3;
	super->acregmax	= 60;
	super->acdirmin	= 30;
	super->acdirmax	= 60;

	retry = -1;

	memset(mnt_pmap, 0, sizeof(*mnt_pmap));
	mnt_pmap->pm_prog = MOUNTPROG;
	memset(nfs_pmap, 0, sizeof(*nfs_pmap));
	nfs_pmap->pm_prog = NFS_PROGRAM;

	new_opts[0] = 0;
	if (hsi_nfs3_parse_options(old_opts, super, &retry, &mnt_server,
				    &nfs_server, new_opts, sizeof(new_opts)))
		goto fail;

	if (hsi_nfsmnt_check_compat(nfs_pmap, mnt_pmap))
		goto fail;

	if (retry == -1) {
		retry = 10000;	/* 10000 mins == ~1 week*/
	}

	if (verbose) {
		INFO("rsize = %d, wsize = %d, timeo = %d, retrans = %d",
		       super->rsize, super->wsize, super->timeo, super->retrans);
		INFO("acreg (min, max) = (%d, %d), acdir (min, max) = (%d, %d)",
		       super->acregmin, super->acregmax, super->acdirmin, super->acdirmax);
		INFO("mountprog = %lu, mountvers = %lu, nfsprog = %lu, nfsvers = %lu",
		       mnt_pmap->pm_prog, mnt_pmap->pm_vers,
		       nfs_pmap->pm_prog, nfs_pmap->pm_vers);
		INFO("mountprot = %lu, mountport = %lu, nfsprot = %lu, nfsvport = %lu",
		       mnt_pmap->pm_prot, mnt_pmap->pm_port,
		       nfs_pmap->pm_prot, nfs_pmap->pm_port);
	}

	memcpy(&save_nfs, nfs_pmap, sizeof(save_nfs));
	memcpy(&save_mnt, mnt_pmap, sizeof(save_mnt));

	timeout = time(NULL) + 60 * retry;
	for (;;) {
		int val = 1;

		if (!fg) {
			sleep(val);	/* 1, 2, 4, 8, 16, 30, ... */
			val *= 2;
			if (val > 30)
				val = 30;
		}

		ret = hsi_nfs3_mount(&mnt_server, &dirname, &mntres);
		if (!ret) {
			/* success! */
			break;
		}

		memcpy(nfs_pmap, &save_nfs, sizeof(*nfs_pmap));
		memcpy(mnt_pmap, &save_mnt, sizeof(*mnt_pmap));

		if (fg) {
			switch(rpc_createerr.cf_stat){
			case RPC_TIMEDOUT:
				break;
			case RPC_SYSTEMERROR:
				if (errno == ETIMEDOUT)
					break;
			default:
				ERR("Mount failed for rpcerr: %d.",
					rpc_createerr.cf_stat);
				goto fail;
			}

			t = time(NULL);
			if (t >= timeout) {
				ERR("Mount failed for rpcerr: %d, timeout.",
					rpc_createerr.cf_stat);;
				goto fail;
			}
			ERR("Mount failed%d.", ret);
			continue;
		}

		if (fg && retry > 0)
			goto fail;
	}

	/* root filehandle */
	{
		mountres3_ok *mountres;
		fhandle3 *fhandle;
		int n_flavors, *flavor;
		if (mntres.fhs_status != 0) {
			ERR("%s: %s:%s failed, reason given by server: %d.",
					progname, hostname, dirname,
					mntres.fhs_status);
			goto fail;
		}

		mountres = &mntres.mountres3_u.mountinfo;
		n_flavors = mountres->auth_flavors.auth_flavors_len;
		flavor = mountres->auth_flavors.auth_flavors_val;
		/* skip flavors */

		fhandle = &mntres.mountres3_u.mountinfo.fhandle;
		memset(&super->root, 0, sizeof(super->root));
		super->root.size = fhandle->fhandle3_len;
		memcpy(super->root.data, (char *) fhandle->fhandle3_val,
				fhandle->fhandle3_len);
	}

	/* nfs3 client */
	super->clntp = hsi_nfs3_clnt_create(&nfs_server);
	if (super->clntp == NULL) {
		hsi_nfs3_unmount(&mnt_server, &dirname);
		goto fail;
	}

	ch = fuse_mount(point, args);
	if (ch == NULL) {
		hsi_nfs3_unmount(&mnt_server, &dirname);
		clnt_destroy(super->clntp);
		goto fail;
	}

	if (!nomtab)
		hsi_add_mtab(spec, point, HSFS_TYPE, super->flags, udata);

	return ch;
fail:
	return NULL;
}