#ifndef SNAPPER_ZFS_H
#define SNAPPER_ZFS_H

#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>

#include "snapper/Filesystem.h"


namespace snapper
{
    class Zfs : public Filesystem
    {
    public:

	static Filesystem* create(const string& fstype, const string& subvolume,
				  const string& root_prefix);

	Zfs(const string& subvolume, const string& root_prefix);

	virtual string fstype() const { return "zfs"; }

	virtual void createConfig() const;
	virtual void deleteConfig() const;

	virtual string snapshotDir(unsigned int num) const;
	virtual string snapshotLvName(unsigned int num) const;

	virtual SDir openInfosDir() const;
	virtual SDir openSnapshotDir(unsigned int num) const;

	virtual void createSnapshot(unsigned int num, unsigned int num_parent,
				    bool read_only, bool quota) const;
	virtual void deleteSnapshot(unsigned int num) const;

	virtual bool isSnapshotMounted(unsigned int num) const;
	virtual void mountSnapshot(unsigned int num) const;
	virtual void umountSnapshot(unsigned int num) const;

	virtual bool isSnapshotReadOnly(unsigned int num) const;

	virtual bool checkSnapshot(unsigned int num) const;

    private:

	string datasetName(unsigned int num) const;
    };

}

#endif
