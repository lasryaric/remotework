# Basically, we have a custom FUSE filesystem which caches read operations from a remote file system like fig or cronut.
#
# Here are more details:
# start command:
# DAIFS_REMOTE=/cronut DAIFS_LOCAL=~/cached_cronut ./passthrough -o allow_other ~/localcronut
# DAIFS_REMOTE: a locally mounted remote filesystem we want to cache
# DAIFS_LOCAL: a directory on the local filesystem where we store the cache of DAIFS_REMOTE
# argv[last] is the directory where the file system we are creating will be mounted.
#
# first we sshfs /cronut and /fig
# then we start 2 passthrough.c instances (one for fig and one for cronut), which act as follow:
#  - mount a new file system at location argv[last]
#  - when it gets a file system call like getattr, open, read etc, it reads all the meta data from DAIFS_REMOTE but when reading the content of a file, it caches it to DAIFS_LOCAL and serves it from there later on. (DAIFS_LOCAL should be a directory on the local file system for fast access).

set -x

#cache timeout
CT=999999999

DAI_SERVER="edamame76.drive.ai"
SSHFS_OPTIONS="-o allow_other,nonempty,cache=yes,cache_timeout=$CT,cache_stat_timeout=$CT,cache_dir_timeout=$CT,cache_link_timeout=$CT"
PASSTHROUGH_OPTIONS="-o allow_other"

CRONUT_MOUNT="/cronut"
FIG_MOUNT="/fig"

LOCAL_FIG_MOUNT="$HOME/driveaifs/mounts/fig"
LOCAL_CRONUT_MOUNT="$HOME/driveaifs/mounts/cronut"

DATA_CRONUT="$HOME/storage2/driveaifs/data/cronut"
DATA_FIG="$HOME/storage2/driveaifs/data/fig"

if [[ $1 = "umount" ]]
then
    sudo umount -f $CRONUT_MOUNT
    sudo umount -f $FIG_MOUNT
    sudo umount -f $LOCAL_FIG_MOUNT
    sudo umount -f $LOCAL_CRONUT_MOUNT
else

#    sshfs $SSHFS_OPTIONS $DAI_SERVER:/cronut /cronut
#    sshfs $SSHFS_OPTIONS $DAI_SERVER:/fig /fig

    DAIFS_REMOTE="$FIG_MOUNT" DAIFS_LOCAL="$DATA_FIG" ./passthrough $PASSTHROUGH_OPTIONS $LOCAL_FIG_MOUNT
    DAIFS_REMOTE="$CRONUT_MOUNT" DAIFS_LOCAL="$DATA_CRONUT" ./passthrough $PASSTHROUGH_OPTIONS $LOCAL_CRONUT_MOUNT
fi
