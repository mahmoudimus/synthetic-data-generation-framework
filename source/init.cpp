/** Implementation of sgfinit. **/

#include "defs.h"

#ifdef SGF_DEBUG
	#pragma message "SGF_DEBUG flag is set -> this is a DEBUG build"
#else
	#pragma message "SGF_DEBUG flag is NOT set -> this is a RELEASE build"
#endif

#include "data.h"
CTVERIFY(MAX_REC_ATTR_COUNT < U16_MAX, maxRecAttrCountheck)

#include "misc.h"

int main(int argc, const char* argv[])
{
	// seed the PRNG
	RNG::getInstance()->defSeedPRNG(); // this seed will be overridden by config

	if(argc != 2)
	{
		cout << "Usage: sgfinit <cfg_file>" << endl << endl;
		cout << "Example: sgfinit workdir/sb.conf" << endl;
		return -1;
	}

	TIC(t0);

	int pid = 0; Config* cfg = NULL; string dataPath = ""; string workdir = ""; string mech = "";
	bool ok = sgfInit(argc, argv, pid, &cfg, dataPath, workdir, mech);
	if(ok == false) { return -1; }

	const u32 count = cfg->get<u32>(CONFIG_COMMON_COUNT);
	const double runtime = cfg->get<double>(CONFIG_COMMON_RUNTIME);

	Metadata* metadata = Metadata::getInstance();
	metadata->initialize(dataPath); // load metadata

	Dataset* store = new Dataset(dataPath, workdir);
	bool wasOnDisk = store->initialize(); // store the data in a compact format

	if(wasOnDisk == false) { LOG(INFO) << "Stored, exiting..."; }

	const double et = TOC(t0);
	LOG(INFO) << "All done in " << ceil(100*et)/100.0 << " seconds.";

	delete store;
	return 0;
}
