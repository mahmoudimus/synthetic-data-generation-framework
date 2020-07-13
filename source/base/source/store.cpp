#include "defs.h"

bool Store::onDisk()
{
	bool ret = true;
	VERIFY(fps.size() > 0);
	foreach_const(set<string>, fps, iter)
	{
		if(fu->fileExists(*iter) == false) { ret = false; break; }
	}
	return ret;
}

bool Store::initialize()
{
	bool wasOnDisk = onDisk();
	if(wasOnDisk == true) { LOG(INFO) << "Loading data..."; load(); LOG(INFO) << "Done."; }
	else
	{
		LOG(INFO) << "Creating and storing data...";
		bool success = create(); if(success == false) { LOG(ERROR) << "create() failed!"; } VERIFY(success == true);
		success = store(); if(success == false) { LOG(ERROR) << "store() failed!"; } VERIFY(success == true);
		LOG(INFO) << "Done.";
	}
	return wasOnDisk;
}
