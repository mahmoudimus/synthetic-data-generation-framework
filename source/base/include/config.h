/** Implements the configuration functionality of the tool through the config file.
 * 	Uses ini.h to parse and store the config file. **/

#ifndef SGFBASE_CONFIG_H_
#define SGFBASE_CONFIG_H_

// the only section
#define CONFIG_SECTION_COMMON "all"

// mandatory
#define CONFIG_COMMON_WORKDIR "workdir"
#define CONFIG_COMMON_DATA_FILEPATH_PREFIX "dataprefix"
#define CONFIG_COMMON_REC_ATTR_COUNT "attrs" /* must be set! */
#define CONFIG_COMMON_MECHANISM "mechanism"

// mechanism values
#define CONFIG_SYNTH_MARGINALS "marginals"
#define CONFIG_SYNTH_SEEDBASED "seedbased"

// basic
#define CONFIG_COMMON_VERBOSE_LEVEL "verbose"
#define CONFIG_COMMON_RUNTIME "runtime"
#define CONFIG_COMMON_RUNTIME_DEFAULT (2 * 3600) /* 2 hours */
#define CONFIG_COMMON_COUNT "count"
#define CONFIG_COMMON_COUNT_DEFAULT (1 << 20) /* ~1 mil */

// we set the def to this invalid value to know that we must set a random seed
#define CONFIG_INVALID_RNG_SEED (0ULL)
#define CONFIG_COMMON_RNG_SEED "rngseed"
#define CONFIG_COMMON_RNG_SEED_DEFAULT CONFIG_INVALID_RNG_SEED

// to save conf
#define CONFIG_COMMON_SAVE_CONFIG "saveconf"
#define CONFIG_COMMON_SAVE_CONFIG_DEFAULT (false)


// privacy parameters for gen model
#define CONFIG_SYNTH_PRIVACY_NOISE_DIST "ndist"
#define CONFIG_SYNTH_PRIVACY_NOISE_DIST_DEFAULT "lap"
#define CONFIG_SYNTH_PRIVACY_NOISE_COMP "ncomp"
#define CONFIG_SYNTH_PRIVACY_NOISE_COMP_DEFAULT "seq"

#define CONFIG_SYNTH_PRIVACY_GENMODEL_LAMBDA "lambda"
#define CONFIG_SYNTH_PRIVACY_GENMODEL_LAMBDA_DEFAULT (60.0)

#define CONFIG_SYNTH_PRIVACY_GENMODEL_BUDGET "budget"
#define CONFIG_SYNTH_PRIVACY_GENMODEL_BUDGET_DEFAULT (1.0)

// other parameters for gen model
#define CONFIG_SYNTH_OMEGA "omega"
#define CONFIG_SYNTH_OMEGA_DEFAULT "m"

#define CONFIG_SYNTH_DIRICHLET_HYPER "dir_hyperp"
#define CONFIG_SYNTH_DIRICHLET_HYPER_DEFAULT (1.0)


// privacy parameters for plausible deniability
#define CONFIG_SYNTH_PARAMS_GAMMA "gamma"
#define CONFIG_SYNTH_PARAMS_GAMMA_DEFAULT (4.0)

#define CONFIG_SYNTH_PARAMS_MAXPS "max_ps"
#define CONFIG_SYNTH_PARAMS_MAXPS_DEFAULT (1000)

#define CONFIG_SYNTH_PARAMS_MAXCHECKPS "max_check_ps"
#define CONFIG_SYNTH_PARAMS_MAXCHECKPS_DEFAULT (100000)

#define CONFIG_SYNTH_PARAMS_RANDOMPS "random_ps"
#define CONFIG_SYNTH_PARAMS_RANDOMPS_DEFAULT (true)


// internal
#define CONFIG_SYNTH_SEEDED_NOISE "seeded_noise"
#define CONFIG_SYNTH_SEEDED_NOISE_DEFAULT (true)


class Config: public Singleton<Config>
{
	friend class Singleton<Config>;

private:

	ini_t* ini;

	Config() : ini(NULL)
	{
		;
	}

	virtual ~Config() {}

public:

#define SET_IF_NO_VAL(_i, _s, _dv) (_i)->set((_s), (_i)->get((_s), (_dv)))

	/** Load configuration from config file. Also sets default values. */
	bool load(const string& fp)
	{
		ini = new ini_t(fp, true);

		// default
		ini->create(CONFIG_SECTION_COMMON);
		SET_IF_NO_VAL(ini, CONFIG_COMMON_WORKDIR, string(""));
		const string workdir = get<string>(CONFIG_COMMON_WORKDIR);
		if(workdir.empty() == true) { LOG(ERROR) << "No working directory specified!"; return false; }

		SET_IF_NO_VAL(ini, CONFIG_COMMON_DATA_FILEPATH_PREFIX, string(""));
		const string datapath = get<string>(CONFIG_COMMON_DATA_FILEPATH_PREFIX);
		if(datapath.empty() == true) { LOG(ERROR) << "No data file path prefix specified!"; return false; }

		SET_IF_NO_VAL(ini, CONFIG_COMMON_VERBOSE_LEVEL, RunParameters::DEBUG_INFO_LEVEL);

		SET_IF_NO_VAL(ini, CONFIG_COMMON_SAVE_CONFIG, CONFIG_COMMON_SAVE_CONFIG_DEFAULT);

		SET_IF_NO_VAL(ini, CONFIG_COMMON_RUNTIME, CONFIG_COMMON_RUNTIME_DEFAULT);
		SET_IF_NO_VAL(ini, CONFIG_COMMON_COUNT, CONFIG_COMMON_COUNT_DEFAULT);

		SET_IF_NO_VAL(ini, CONFIG_SYNTH_SEEDED_NOISE, CONFIG_SYNTH_SEEDED_NOISE_DEFAULT);

		SET_IF_NO_VAL(ini, CONFIG_COMMON_RNG_SEED, CONFIG_COMMON_RNG_SEED_DEFAULT);

		SET_IF_NO_VAL(ini, CONFIG_SYNTH_PRIVACY_NOISE_DIST, CONFIG_SYNTH_PRIVACY_NOISE_DIST_DEFAULT);
		SET_IF_NO_VAL(ini, CONFIG_SYNTH_PRIVACY_NOISE_COMP, CONFIG_SYNTH_PRIVACY_NOISE_COMP_DEFAULT);
		SET_IF_NO_VAL(ini, CONFIG_SYNTH_PRIVACY_GENMODEL_LAMBDA, CONFIG_SYNTH_PRIVACY_GENMODEL_LAMBDA_DEFAULT);
		SET_IF_NO_VAL(ini, CONFIG_SYNTH_PRIVACY_GENMODEL_BUDGET, CONFIG_SYNTH_PRIVACY_GENMODEL_BUDGET_DEFAULT);

		SET_IF_NO_VAL(ini, CONFIG_SYNTH_OMEGA, CONFIG_SYNTH_OMEGA_DEFAULT);

		SET_IF_NO_VAL(ini, CONFIG_SYNTH_DIRICHLET_HYPER, CONFIG_SYNTH_DIRICHLET_HYPER_DEFAULT);

		const string mech = ini->get(CONFIG_COMMON_MECHANISM, string(""));
		if(mech.compare(CONFIG_SYNTH_MARGINALS) == 0 || mech.compare(CONFIG_SYNTH_SEEDBASED) == 0)
		{
			SET_IF_NO_VAL(ini, CONFIG_SYNTH_OMEGA, CONFIG_SYNTH_OMEGA_DEFAULT);

			if(mech.compare(CONFIG_SYNTH_SEEDBASED) == 0)
			{
				SET_IF_NO_VAL(ini, CONFIG_SYNTH_PARAMS_GAMMA, CONFIG_SYNTH_PARAMS_GAMMA_DEFAULT);
				SET_IF_NO_VAL(ini, CONFIG_SYNTH_PARAMS_MAXPS, CONFIG_SYNTH_PARAMS_MAXPS_DEFAULT);
				SET_IF_NO_VAL(ini, CONFIG_SYNTH_PARAMS_MAXCHECKPS, CONFIG_SYNTH_PARAMS_MAXCHECKPS_DEFAULT);
				SET_IF_NO_VAL(ini, CONFIG_SYNTH_PARAMS_RANDOMPS, CONFIG_SYNTH_PARAMS_RANDOMPS_DEFAULT);
			}
		}
		else
		{
			LOG(INFO) << "Unrecognized mechanism: " << mech << ", exiting...";
			return false;
		}

		// number of attributes for each record
		const string recAttrCountStr = get<string>(CONFIG_COMMON_REC_ATTR_COUNT);
		const int64 recAttrCount = parseString<int64>(recAttrCountStr);
		if(recAttrCountStr.empty() || recAttrCount <= 0 || recAttrCount >= UINT32_MAX)
		{
			LOG(INFO) << "Invalid number of attributes per record, exiting...";
			return false;
		}
		set<u64>(CONFIG_COMMON_REC_ATTR_COUNT, (u64)recAttrCount);


		// by default don't save!!
		if(get<bool>(CONFIG_COMMON_SAVE_CONFIG) == true){ ini->save(); }

		if(workdir.empty() == false && mech.empty() == false)
		{
			stringstream ssf(""); ssf << workdir << "/gen";
			{
				stringstream ssm(""); ssm << "mkdir -p " << ssf.str();
				int rv = system(ssm.str().c_str());
				LOG(INFO) << "Running system command '" << ssm.str() << "' -- rv: " << rv;
			}
		}
		else
		{
			LOG(INFO) << "Invalid mechanism, exiting...";
			return false;
		}

		return true;
	}

	/** Print the configuration to log. */
	void print()
	{
		LOG(INFO) << "Config:";
		for(auto i: ini->sections)
		{
			LOG(INFO) << "[" << i.first << "]";
			for(auto j: *i.second)
			{
				LOG(INFO) << "\t" << j.first << "=" << j.second;
			}
		}
	}

	template <typename T> T get(const string& key) { return ini->get(key, T()); }
	template <typename T> T getOrDef(const string& key, T& def) { return ini->get(key, def); }

	template <typename T> void set(const string& key, T val) { ini->set(key, val); }
};

#endif // SGFBASE_CONFIG_H_
