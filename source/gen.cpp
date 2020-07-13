/** Implementation of sgfgen. **/

#include "defs.h"

#ifdef SGF_DEBUG
	#pragma message "SGF_DEBUG flag is set -> this is a DEBUG build"
#else
	#pragma message "SGF_DEBUG flag is NOT set -> this is a RELEASE build"
#endif

//#define TEST_CODE 1

#include "gen.h"

CTVERIFY(MAX_REC_ATTR_COUNT < U16_MAX, maxRecAttrCountheck)

#include "misc.h"


int main(int argc, const char* argv[])
{
	// seed the PRNG
	RNG::getInstance()->defSeedPRNG(); // this seed will be overridden by config

	if(argc != 2)
	{
		cout << "Usage: sgfgen <cfg_file>" << endl << endl;
		cout << "Example: sgfgen workdir/sb.conf" << endl;
		return -1;
	}

	RTM* rtm = RTM::getInstance();
	TIC(t0);

	int pid = 0; Config* cfg = NULL; string dataPath = ""; string workdir = ""; string mech = "";
	bool ok = sgfInit(argc, argv, pid, &cfg, dataPath, workdir, mech);
	if(ok == false) { return -1; }

	const u32 count = cfg->get<u32>(CONFIG_COMMON_COUNT);
	const double runtime = cfg->get<double>(CONFIG_COMMON_RUNTIME);

	Metadata* metadata = Metadata::getInstance();
	metadata->initialize(dataPath);

	Dataset* store = new Dataset(dataPath, workdir);
	bool initDone = store->onDisk();

	if(initDone == false)
	{
		LOG(ERROR) << "Initialization not performed (run sgfinit first)!";
		delete store;
		return 0;
	}

	ok = store->load();
	if(ok == false)
	{
		LOG(ERROR) << "Could not load preprocessed data (try to re-run sgfinit)!";
		delete store;
		return 0;
	}

	vector<prec>& dataset = store->getRecords();
	vector<prec>& stats = store->getStats();

	RecordLineWriter* writer = new RecordLineWriter();

	stringstream outprefss("");
	outprefss << workdir << "/gen";
	const string genpref = outprefss.str();

	// validate params
	string ndist = ""; string ncomp = "";
	{
		ndist = cfg->get<string>(CONFIG_SYNTH_PRIVACY_NOISE_DIST);
		if(ndist.compare("no") != 0 && ndist.compare("none") != 0 && ndist.compare("lap") != 0 && ndist.compare("geom") != 0)
		{
			LOG(ERROR) << "Unrecognized noise distribution: " << ndist << ", exiting...";
			return -1;
		}

		ncomp = cfg->get<string>(CONFIG_SYNTH_PRIVACY_NOISE_COMP);
		if(ncomp.compare("def") != 0  && ncomp.compare("seq") != 0 && ncomp.find("adv") != 0)
		{
			LOG(ERROR) << "Unrecognized noise composition strategy: " << ncomp << ", exiting...";
			return -1;
		}
	}
	const double log2delta = cfg->get<double>(CONFIG_SYNTH_PRIVACY_GENMODEL_LAMBDA);
	if(log2delta <= 0)
	{
		LOG(ERROR) << "Invalid value for lambda = log2(delta) of the generative model (must be >0), exiting...";
		return -1;
	}

	const double dirhyper = cfg->get<double>(CONFIG_SYNTH_DIRICHLET_HYPER);
	if(dirhyper < 0)
	{
		LOG(ERROR) << "Invalid value for dirichlet parameter of the generative model (must be >=0), exiting...";
		return -1;
	}

	// use either the seedbased model or the marginals
	bool success = false;
	if(mech.compare("seedbased") == 0)
	{
		if(cfg->get<bool>(CONFIG_SYNTH_SEEDED_NOISE) == false)
		{ LOG(ERROR) << "Invalid value for seeded noise (=0), must be true (=1) for mechanism " << mech << ", exiting..."; return -1;  }
		VERIFY(cfg->get<bool>(CONFIG_SYNTH_SEEDED_NOISE) == true); // must be true!

		const string& omega = cfg->get<string>(CONFIG_SYNTH_OMEGA);
		if(omega.compare("m") != 0)
		{
			const int32 v = parseString<int32>(omega);
			if(v < 0 || v > REC_ATTR_COUNT) { LOG(ERROR) << "Invalid omega: " << omega << ", exiting..."; return -1; }
		}

		synthparams params;
		params.fakesPerSeed = 1; // this can be changed if needed.
		params.count = count;
		params.runtime = runtime;

		params.gam = cfg->get<double>(CONFIG_SYNTH_PARAMS_GAMMA);

		params.maxCheckPS = cfg->get<u32>(CONFIG_SYNTH_PARAMS_MAXCHECKPS);
		params.maxPS = cfg->get<u32>(CONFIG_SYNTH_PARAMS_MAXPS);
		params.randomPSOrder = cfg->get<bool>(CONFIG_SYNTH_PARAMS_RANDOMPS);

		GenerativeModel<rec>* gen = new SeedBasedGenerativeModel(stats); // give the 'stats' dataset
		Synthesizer<rec>* synth = new Synthesizer<rec>(dataset, gen);			   // give the 'training' dataset

		RecordOutputter* synthOuttputter = new RecordOutputter(writer, genpref, pid);

		success = synth->run(&params, synthOuttputter); // run synthesis

		delete gen; delete synth; delete synthOuttputter;
	}
	else if(mech.compare("marginals") == 0)
	{
		if(cfg->get<bool>(CONFIG_SYNTH_SEEDED_NOISE) == false)
		{ LOG(ERROR) << "Invalid value for seeded noise (=0), must be true (=1) for mechanism " << mech << ", exiting..."; return -1;  }
		VERIFY(cfg->get<bool>(CONFIG_SYNTH_SEEDED_NOISE) == true); // must be true!

		const string& omega = cfg->get<string>(CONFIG_SYNTH_OMEGA);
		if(omega.compare("m") != 0)
		{ LOG(ERROR) << "Invalid use of parameter omega (=" << omega << ") for marginals model (remove omega or set omega=m and try again), exiting..."; return -1; }

		GenerativeModel<rec>* gen = NULL;

		const bool uniform = mech.compare("uniform") == 0;
		gen = new MarginalsGenerativeModel(stats, uniform); // give stats

		Synthesizer<rec>* synth = new Synthesizer<rec>(dataset, gen);			   // give the 'training' dataset

		RecordOutputter* synthOuttputter = new RecordOutputter(writer, genpref, pid);

		synthparams params;
		params.fakesPerSeed = 1;
		params.count = count;
		params.runtime = runtime;

		// these parameters don't matter (they are ignored by marginals synthesis)
		params.gam = 2.0;
		params.maxCheckPS = 0;
		params.maxPS = 0;
		params.randomPSOrder = false;

		success = synth->run(&params, synthOuttputter); // run synthesis

		delete gen; delete synth; delete synthOuttputter;
	}
	else { LOG(ERROR) << "Invalid arguments or unrecognized mechanism, exiting..."; return -1; }

	if(success == false) { LOG(ERROR) << "Failed to run " << mech << " SGF, exiting..."; return -1; }

	rtm->dumpToLog(); // write timing measurements to log

	const double et = TOC(t0);
	LOG(INFO) << "All done in " << ceil(100*et)/100.0 << " seconds.";

	delete store; delete writer;
	return 0;
}
