/** Common functionality for sgfinit, sgfgen, sgfextract. **/
#define ELPP_DEBUG_ASSERT_FAILURE 1
#define ELPP_NO_DEFAULT_LOG_FILE 1
#define ELPP_THREAD_SAFE 1

INITIALIZE_EASYLOGGINGPP

/** Initialize the logging functionality using easylogging++. */
static void initializeLog(int argc, const char* argv[], const string& workdir)
{
#define BUFFER_MAX_SIZE 1024
	char buffer[BUFFER_MAX_SIZE + 1];
	const char* p = getcwd(buffer, BUFFER_MAX_SIZE);
	if(p != buffer) { LOG(ERROR) << "getcwd() returned an error."; exit(-1); }

	stringstream ss(""); ss << buffer;
	const string cwd = ss.str();

	ss.str("");
	ss << workdir << "/logs/" << getPID() << ".log";
	const string logfp = ss.str();

	el::Configurations conf("./log.conf"); // Load configuration from file
	conf.setGlobally(el::ConfigurationType::Filename, logfp);

	el::Loggers::reconfigureAllLoggers(conf); // Actually reconfigure all loggers instead
	el::Loggers::addFlag(el::LoggingFlag::NewLineForContainer);
	el::Loggers::addFlag(el::LoggingFlag::ImmediateFlush);

	START_EASYLOGGINGPP(argc, argv);
}

/** Initialization for any sgfXXX. Parse command-line arguments and config file. */
static bool sgfInit(int argc, const char* argv[], int& pid, Config** cfgOut, string& dataPath, string& workdir, string& mech)
{
	if(argc <= 1) { return false; }

	string cfgfp = argv[1];

	Config* cfg = Config::getInstance(); *cfgOut = cfg;
	bool ok = cfg->load(cfgfp);

	if(ok == false)
	{
		cerr << "Invalid config file: " << cfgfp << ", exiting..." << endl;
		return false;
	}

	dataPath = cfg->get<string>(CONFIG_COMMON_DATA_FILEPATH_PREFIX);

	workdir = cfg->get<string>(CONFIG_COMMON_WORKDIR);
	initializeLog(argc, argv, workdir);

	cfg->print();

	RunParameters* rp = RunParameters::getInstance();
	rp->setDebugLevel(cfg->get<u16>(CONFIG_COMMON_VERBOSE_LEVEL));

	// get and set the number of attributes per record
	const u64 recAttrCount = cfg->get<u64>(CONFIG_COMMON_REC_ATTR_COUNT);
	if(recAttrCount == 0 || recAttrCount > MAX_REC_ATTR_COUNT)
	{
		cerr << "Invalid number of attributes, exiting...";
		return -1;
	}
	rp->setRecAttrCount((u16)recAttrCount); // set it once, we won't change this again.

	mech = cfg->get<string>(CONFIG_COMMON_MECHANISM);

	pid = getPID();
	LOG(INFO) << "Running (pid: " << pid << ") on " << dataPath.c_str() << " in " << workdir.c_str() << " (mech: " << mech << ")";

	const u64 rngSeed = cfg->get<u64>(CONFIG_COMMON_RNG_SEED);
	if(rngSeed == CONFIG_INVALID_RNG_SEED)
	{
		LOG(INFO) << "[RNG] No seed (or 0 -- invalid) specified, using a random seed from the underlying RNG.";
		RNG::getInstance()->defSeedPRNG();
	}
	else
	{
		LOG(INFO) << "[RNG] Seed specified to be " << rngSeed << ".";
		RNG::getInstance()->seedPRNG(rngSeed);
	}

	return true;
}
