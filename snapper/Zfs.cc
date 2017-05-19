#include "config.h"

#include "snapper/Zfs.h"
#include <libzfs_core.h>
#include <sys/nvpair.h>

namespace snapper
{
    using namespace std;

    struct NvList {
        nvlist_t *raw;

        NvList() {
            if (nvlist_alloc(&raw, 0, 0))
                throw std::runtime_error("nvlist allocation failed");
        }

        ~NvList() {
           nvlist_free(raw); 
        }

        void push(const string &key) {
            if (nvlist_add_boolean(raw, key.cstr()))
                throw std::runtime_error("nvlist add boolean failed");
        }
    }

    std::string Zfs::datasetName(unsigned int num)
    {
        // XXX: this is totally wrong. fix it.
        return "tank-snapper-" + num;
    }

    Filesystem*
    Zfs::create(const string& fstype, const string& subvolume, const string& root_prefix)
    {
	if (fstype == "zfs")
	    return new Zfs(subvolume, root_prefix);

	return NULL;
    }


    Zfs::Zfs(const string& subvolume, const string& root_prefix)
	: Filesystem(subvolume, root_prefix)
    {
    }


    void
    Zfs::evalConfigInfo(const ConfigInfo& config_info)
    {
    }

    void
    Zfs::createConfig() const
    {
	SDir subvolume_dir = openSubvolumeDir();

	try
	{
	    create_subvolume(subvolume_dir.fd(), ".snapshots");
	}
	catch (const runtime_error_with_errno& e)
	{
	    y2err("create subvolume failed, " << e.what());

	    switch (e.error_number)
	    {
		case EEXIST:
		    SN_THROW(CreateConfigFailedException("creating btrfs subvolume .snapshots failed "
							 "since it already exists"));

		default:
		    SN_THROW(CreateConfigFailedException("creating btrfs subvolume .snapshots failed"));
	    }
	}

	SFile x(subvolume_dir, ".snapshots");
	struct stat stat;
	if (x.stat(&stat, 0) == 0)
	    x.chmod(stat.st_mode & ~0027, 0);
    }


    void
    Zfs::deleteConfig() const
    {
	SDir subvolume_dir = openSubvolumeDir();

	try
	{
	    delete_subvolume(subvolume_dir.fd(), ".snapshots");
	}
	catch (const runtime_error& e)
	{
	    y2err("delete subvolume failed, " << e.what());
	    SN_THROW(DeleteConfigFailedException("deleting btrfs snapshot failed"));
	}
    }

    string
    Zfs::snapshotDir(unsigned int num) const
    {
	return (subvolume == "/" ? "" : subvolume) + "/.snapshots/" + decString(num) +
	    "/snapshot";
    }

    SDir
    Zfs::openSubvolumeDir() const
    {
	SDir subvolume_dir = Filesystem::openSubvolumeDir();

	struct stat stat;
	if (subvolume_dir.stat(&stat) != 0)
	{
	    SN_THROW(IOErrorException("stat on subvolume directory failed"));
	}

	if (!is_subvolume(stat))
	{
	    SN_THROW(IOErrorException("subvolume is not a btrfs subvolume"));
	}

	return subvolume_dir;
    }


    SDir
    Zfs::openInfosDir() const
    {
	SDir subvolume_dir = openSubvolumeDir();
	SDir infos_dir(subvolume_dir, ".snapshots");

	struct stat stat;
	if (infos_dir.stat(&stat) != 0)
	{
	    SN_THROW(IOErrorException("stat on info directory failed"));
	}

	if (!is_subvolume(stat))
	{
	    SN_THROW(IOErrorException(".snapshots is not a btrfs subvolume"));
	}

	if (stat.st_uid != 0)
	{
	    y2err(".snapshots must have owner root");
	    SN_THROW(IOErrorException(".snapshots must have owner root"));
	}

	if (stat.st_gid != 0 && stat.st_mode & S_IWGRP)
	{
	    y2err(".snapshots must have group root or must not be group-writable");
	    SN_THROW(IOErrorException(".snapshots must have group root or must not be group-writable"));
	}

	if (stat.st_mode & S_IWOTH)
	{
	    y2err(".snapshots must not be world-writable");
	    SN_THROW(IOErrorException(".snapshots must not be world-writable"));
	}

	return infos_dir;
    }


    SDir
    Zfs::openSnapshotDir(unsigned int num) const
    {
	SDir info_dir = openInfoDir(num);
	SDir snapshot_dir(info_dir, "snapshot");

	return snapshot_dir;
    }


    SDir
    Zfs::openGeneralDir() const
    {
	return openInfosDir();
    }


    void
    Zfs::createSnapshot(unsigned int num, unsigned int num_parent, bool read_only,
			  bool quota) const
    {
        // all zfs snapshots are read_only
        // I'm not sure what snapshots with num_parent != 0 would imply
	if (num_parent != 0 || !read_only)
	    throw std::logic_error("not implemented");

        std::string dn = datasetName(num);

        int r;

        try {
            NvList snaps;
            snaps.push(dn);

            r = lzc_snapshot(snaps.raw, NULL, NULL);
        } catch(e) {
            y2err("failed to create snapshot, " << e.what());
            SN_THROW(CreateSnapshotFailedException());
        }

        if (r) {
            y2err("failed to create snapshot, " << r);
            SN_THROW(CreateSnapshotFailedException());
        }
    }

    void
    Zfs::deleteSnapshot(unsigned int num) const
    {
	// lzc_destroy_snaps(nv_list_of_snaps, bool_defer, nv_list_ref_errs)

	std::string dn = datasetName(num);
        NvList snaps;
        snaps.push(dn.cstr());

	// TODO: consider appropriateness of defer boolean here
	r = lzc_destroy_snaps(snaps.raw, false, NULL);
	if (r) {
		y2err("failed to  delete snapshot, " << r);
		SN_THROW(DeleteSnapshotFailedException());
	}
    }

    bool
    Zfs::isSnapshotMounted(unsigned int num) const
    {
        // snapshots are always "mounted" below <volume>/.zfs/snapshot
        return true;
    }

    void
    Zfs::mountSnapshot(unsigned int num) const
    {
        // snapshots are always "mounted" below <volume>/.zfs/snapshot
    }

    void
    Zfs::umountSnapshot(unsigned int num) const
    {
        // snapshots are always "mounted" below <volume>/.zfs/snapshot
    }


    bool
    Zfs::isSnapshotReadOnly(unsigned int num) const
    {
        // all zfs snapshots are read-only
        return true;
    }


    bool
    Zfs::checkSnapshot(unsigned int num) const
    {
	std::string dn = datasetName(num);
	if (lzc_exists(dn.cstr()))
		return true;
	return false;
    }
}
