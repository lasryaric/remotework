/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/** @file
 *
 * This file system mirrors the existing file system hierarchy of the
 * system, starting at the root file system. This is implemented by
 * just "passing through" all requests to the corresponding user-space
 * libc functions. Its performance is terrible.
 *
 * Compile with
 *
 *     gcc -Wall passthrough.c `pkg-config fuse3 --cflags --libs` -o passthrough
 *
 * ## Source code ##
 * \include passthrough.c
 */


#define FUSE_USE_VERSION 31

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif


#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#define MAX_PATH_SIZE 4096

static char *remote_prefix;
static char *local_prefix;

struct dai_paths {
	char path[MAX_PATH_SIZE];
	char remote_path[MAX_PATH_SIZE];
	char local_path[MAX_PATH_SIZE];
};

typedef struct dai_paths dai_paths;



void copy_file(char *source, char *dest);
void mkdir_recursive(char* path);

static void get_full_path(const char *path, char *prefix, char* dest) {
    strcat(dest, prefix);
    strcat(dest, path);
}

static void get_remote_path(const char *path, char* dest) {
    get_full_path(path, remote_prefix, dest);
}

static void get_local_path(const char *path, char* dest) {
    get_full_path(path, local_prefix, dest);
}

static void get_paths(dai_paths** paths, const char* path) {
	*paths = malloc(sizeof(dai_paths));
	memset(*paths, 0, sizeof(dai_paths));
	get_remote_path(path, (*paths)->remote_path);
	get_local_path(path, (*paths)->local_path);
	strcat((*paths)->path, path);	
	
}

static dai_paths* mirror_file(const char* path) {
	struct stat stbuf;
	dai_paths* paths = 0;
	get_paths(&paths, path);

	printf("-- mirror_file() for path : %s\n", path);
    if (-1 == lstat(paths->local_path, &stbuf)) {
		printf("does not exists: %s\n", paths->local_path);
		struct stat stbuf_remote;
		if (lstat(paths->remote_path, &stbuf_remote) != -1) {
			printf("exists: %s\n", paths->remote_path);
			if (S_ISREG(stbuf_remote.st_mode)) {
        		copy_file(paths->remote_path, paths->local_path);
			} else if (S_ISDIR(stbuf_remote.st_mode)) {
				mkdir_recursive(paths->local_path);
			}
    	}
	}

	return paths;	
}

int is_file(char* path) {
    struct stat stbuf;

    if (lstat(path, &stbuf) == -1) {
        printf("is_file can't lstat %s\n", path);
    } else {
        if (S_ISREG(stbuf.st_mode)) {
            return 1;
        }
    }

    return 0;
}


static void free_paths(dai_paths *paths) {
	free(paths);
}

void copy_file(char *source, char *dest)
{
    int childExitStatus;
    pid_t pid;
    int status;
	(void)status;
    if (!source || !dest) {
        /* handle as you wish */
    }

    pid = fork();

    if (pid == 0) { /* child */
		printf("executing : /bin/cp %s %s\n", source, dest);
        execl("/bin/cp", "/bin/cp", source, dest, (char *)0);
    }
    else if (pid < 0) {
        /* error - couldn't start process - you decide how to handle */
    }
    else {
        /* parent - wait for child - this has all error handling, you
         * could just call wait() as long as you are only expecting to
         * have one child process at a time.
         */
        pid_t ws = waitpid( pid, &childExitStatus, 0);
        if (ws == -1)
        { /* error - handle as you wish */
        }

        if( WIFEXITED(childExitStatus)) /* exit code in childExitStatus */
        {
            status = WEXITSTATUS(childExitStatus); /* zero is normal exit */
            /* handle non-zero as you wish */
        }
        else if (WIFSIGNALED(childExitStatus)) /* killed */
        {
        }
        else if (WIFSTOPPED(childExitStatus)) /* stopped */
        {
        }
    }
}

void mkdir_recursive(char *path)
{
    int childExitStatus;
    pid_t pid;
    int status;
	(void)status;

    pid = fork();

    if (pid == 0) { /* child */
		printf("executing : /bin/mkdir -p %s\n", path);
        execl("/bin/mkdir", "/bin/mkdir", path, (char *)0);
    }
    else if (pid < 0) {
        /* error - couldn't start process - you decide how to handle */
    }
    else {
        /* parent - wait for child - this has all error handling, you
         * could just call wait() as long as you are only expecting to
         * have one child process at a time.
         */
        pid_t ws = waitpid( pid, &childExitStatus, 0);
        if (ws == -1)
        { /* error - handle as you wish */
        }

        if( WIFEXITED(childExitStatus)) /* exit code in childExitStatus */
        {
            status = WEXITSTATUS(childExitStatus); /* zero is normal exit */
            /* handle non-zero as you wish */
        }
        else if (WIFSIGNALED(childExitStatus)) /* killed */
        {
        }
        else if (WIFSTOPPED(childExitStatus)) /* stopped */
        {
        }
    }
}

static void *xmp_init(struct fuse_conn_info *conn,
		      struct fuse_config *cfg)
{
	(void) conn;
	cfg->use_ino = 1;

	/* Pick up changes from lower filesystem right away. This is
	   also necessary for better hardlink support. When the kernel
	   calls the unlink() handler, it does not know the inode of
	   the to-be-removed entry and can therefore not invalidate
	   the cache of the associated inode - resulting in an
	   incorrect st_nlink value being reported for any remaining
	   hardlinks to this inode. */
	cfg->entry_timeout = 0;
	cfg->attr_timeout = 0;
	cfg->negative_timeout = 0;

	return NULL;
}

static int xmp_getattr(const char *path, struct stat *stbuf,
		       struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	(void) fi;
	int res;

	dai_paths* paths = mirror_file(path);
    if (is_file(paths->remote_path) == 1) {
	    res = lstat(paths->local_path, stbuf);
    } else {
        res = lstat(paths->remote_path, stbuf);
    }
	free_paths(paths);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	printf("%s\n", __func__);
	int res;
    
    dai_paths* paths = mirror_file(path);
    if (is_file(paths->remote_path)) {
        res = access(paths->remote_path, mask);
    } else {
        res = access(paths->remote_path, mask);
    }
    free_paths(paths);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	printf("%s\n", __func__);
	int res;
    dai_paths* paths = 0;
    get_paths(&paths, path);

	res = readlink(paths->remote_path, buf, size - 1);
    free_paths(paths);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,
		       enum fuse_readdir_flags flags)
{
	printf("%s\n", __func__);
	/*dai_paths* paths = get_paths(path);
	size_t cache_size = MAX_PATH_SIZE * 2;
	struct st_buf stbuf;
	char* cache_path = malloc(sizeof(char) * cache_size);
	memset(cache_path, 0, sizeof(char) * cache_size);
	strcat(cache_path, local_prefix);
	strcat(cache_path, "/.daifs_cache/");
	strcat(cache_path, paths->path);
	if (lstat(cache_path, &stbuf) == -1) {
		mkdir_recursive(cache_path);
	}

	char *cache_file = malloc(sizeof(char) * cache_size);
	memset(cache_file, 0, sizeof(char) * cache_size);
	strcat(cache_file, cache_path);
	strcat(cache_file, "/data.bin");
	
	if (lstat(cache_file, &st_buf) != -1) {
		int fd = open(cache_file, 0, O_RD);
		if (fd == -1) {
			printf("error opening cache file : %s\n", cache_file);
		} else {
			size_t total_nb_read = 0;
			while ((size_t nb_read = read(fd, &buf[total_nb_read], 512)) > 0) {
			total_nb_read += nb_read;
			}  
			if (nb_read == -1) {
				printf("Error reading cache file %s\n", cache_file);
			}
		}
		free(cache_file);
		free(cache_path);
		free_paths(paths);
		
		
		return 0;
	}
	*/
	
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;
	(void) flags;
    dai_paths* paths = 0;
    get_paths(&paths, path);
    printf("-- readdir(%s)\n", path);
    printf("remote_path: %s\nlocal_path: %s\n\n", paths->remote_path, paths->local_path);
	dp = opendir(paths->remote_path);
	if (dp == NULL) {
        free_paths(paths);

		return -errno;
    }

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0, 0))
			break;
	}
	

	closedir(dp);
	/*
	int fd = open(cache_file, O_WRONLY, O_CREAT);
	if (fd == -1) {
		printf("can't open cache file for writing : %s\n", cache_file);
	} else {
		off_t fsize;
	
		fsize = lseek(fd, 0, SEEK_END);
		write(fd, buf, fsize);
	}
	*/

    free_paths(paths);

	return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	printf("%s\n", __func__);
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	printf("%s\n", __func__);
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	printf("%s\n", __func__);
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	printf("%s\n", __func__);
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	printf("%s\n", __func__);
	int res;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rename(const char *from, const char *to, unsigned int flags)
{
	printf("%s\n", __func__);
	int res;

	if (flags)
		return -EINVAL;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	printf("%s\n", __func__);
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *path, mode_t mode,
		     struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	(void) fi;
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid,
		     struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	(void) fi;
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size,
			struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	int res;

	if (fi != NULL)
		res = ftruncate(fi->fh, size);
	else
		res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2],
		       struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	(void) fi;
	int res;

	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}
#endif

static int xmp_create(const char *path, mode_t mode,
		      struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	int res;

	res = open(path, fi->flags, mode);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	printf("open()\n");
	int res;

	dai_paths* paths = mirror_file(path);
	res = open(paths->local_path, fi->flags);
	free_paths(paths);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	printf("read()\n");
	int fd;
	int res;

    dai_paths* paths = mirror_file(path);

	if(fi == NULL)
		fd = open(paths->local_path, O_RDONLY);
	else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}

static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	int fd;
	int res;

	(void) fi;
	if(fi == NULL)
		fd = open(path, O_WRONLY);
	else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	printf("%s\n", __func__);
	printf("statfs()");
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	(void) path;
	close(fi->fh);
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	printf("%s\n", __func__);
	int fd;
	int res;

	(void) fi;

	if (mode)
		return -EOPNOTSUPP;

	if(fi == NULL)
		fd = open(path, O_WRONLY);
	else
		fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = -posix_fallocate(fd, offset, length);

	if(fi == NULL)
		close(fd);
	return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	printf("%s\n", __func__);
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	printf("%s\n", __func__);
	printf("getXattr()\n");
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
	printf("%s\n", __func__);
	printf("listXattr");
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
	printf("%s\n", __func__);
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

#ifdef HAVE_COPY_FILE_RANGE
static ssize_t xmp_copy_file_range(const char *path_in,
				   struct fuse_file_info *fi_in,
				   off_t offset_in, const char *path_out,
				   struct fuse_file_info *fi_out,
				   off_t offset_out, size_t len, int flags)
{
	printf("%s\n", __func__);
	int fd_in, fd_out;
	ssize_t res;

	if(fi_in == NULL)
		fd_in = open(path_in, O_RDONLY);
	else
		fd_in = fi_in->fh;

	if (fd_in == -1)
		return -errno;

	if(fi_out == NULL)
		fd_out = open(path_out, O_WRONLY);
	else
		fd_out = fi_out->fh;

	if (fd_out == -1) {
		close(fd_in);
		return -errno;
	}

	res = copy_file_range(fd_in, &offset_in, fd_out, &offset_out, len,
			      flags);
	if (res == -1)
		res = -errno;

	close(fd_in);
	close(fd_out);

	return res;
}
#endif

static struct fuse_operations xmp_oper = {
	.init           = xmp_init,
	// todo
	.getattr	= xmp_getattr,
	//todo
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.readdir	= xmp_readdir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= xmp_utimens,
#endif
	//todo
	.open		= xmp_open,
	.create 	= xmp_create,
	//todo
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.release	= xmp_release,
	.fsync		= xmp_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= xmp_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
#ifdef HAVE_COPY_FILE_RANGE
	.copy_file_range = xmp_copy_file_range,
#endif
};

int main(int argc, char *argv[])
{
    remote_prefix = malloc(sizeof(char) * MAX_PATH_SIZE);
    local_prefix = malloc(sizeof(char) * MAX_PATH_SIZE);
    strcat(remote_prefix, getenv("DAIFS_REMOTE"));
    strcat(local_prefix, getenv("DAIFS_LOCAL"));
    (void)remote_prefix;
    (void)(local_prefix);
	umask(0);
    printf("working with :\nremote_prefix:%s\nlocal_prefix:%s", remote_prefix, local_prefix);
	return fuse_main(argc, argv, &xmp_oper, NULL);
}
