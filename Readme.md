# FUSE cached-mirror file system

This is a FUSE (user space) filesystem that caches read() operations from any other file system. This is to help working remotely with large scale remote file systems.
Example: you run (localy) an application that loads files from a remote file system (like GlusterFS or NFS). When working on a local network, everything is fast but when working remotely, loading all the remote files again and again can be very costly, this file system mirror the remote file system and caches the file on the local disk so that it can survive reboots.

### Here are more details:
```
start command: DAIFS_REMOTE=/cronut DAIFS_LOCAL=~/cached_cronut ./passthrough -o allow_other ~/localcronut

DAIFS_REMOTE: a locally mounted remote filesystem we want to cache
DAIFS_LOCAL: a directory on the local filesystem where we store the cache of DAIFS_REMOTE
argv[last] is the directory where the file system we are creating will be mounted. The combination of ENV and argv is because of the way the fuse library parses argv. Sorry for this.
```
