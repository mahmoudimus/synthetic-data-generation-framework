/** Implementation of sgfextract. **/

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

#define DEFAULT_ADVC_LAMBDA (80.0)

/** Computes the epsilon,delta for differential privacy according to Theorem 1 of the VLDB paper.
 * Also, determines the best composition strategy for the n synthetics records (sequential composition or advanced composition).
 */
void computeDPBudget(const double n, const double gam, const double k, const double t, const double eps0, double& epsv, double& deltav, string& compstr, const double lambda = DEFAULT_ADVC_LAMBDA)
{
	VERIFY(t < k);

	const double epsp = eps0 + log(1+gam/t);
	const double deltap = exp(-eps0 * (k-t));

	const double seqceps = n*epsp;
	const double seqcdelta = n*deltap;

	const double advlambda = lambda;
	const double advinvlndelta = advlambda/log2(exp(1));

	const double advcadddelta = pow(2.0, -advlambda);
	const double advcdelta = seqcdelta + advcadddelta;
	const double advceps = epsp * sqrt(2*n*advinvlndelta) + n * epsp * (exp(epsp) - 1.0);

	if(seqceps < advceps)
	{
		compstr = "seq. comp.";
		epsv = seqceps;
		deltav = seqcdelta;
	}
	else
	{
		compstr = "adv. comp.";
		epsv = advceps;
		deltav = advcdelta;
	}

	if(deltav > 1.0) { deltav = 1.0; }
}

/** Compute adequate values for the parameters to meet the target privacy guarantee given lambda and maxEps. */
void computeParamsFromLambda(const double lambda, const double maxEps, const double gam, double& k, double& s, double& eps0, const u32 outputCount)
{
	VERIFY(outputCount > 0);
	const double lambdabe = ((lambda+1)/log2(exp(1)))+log(outputCount);

	double c = ceil(sqrt((double)outputCount)); double epsv = 0.0; u32 iter = 0;
	do
	{
		s = ceil(c * lambdabe);
		k = s + ceil(c * gam);
		const double t = k - s;

		eps0 = lambdabe / s;
		const double epsk = eps0 + log(1.0 + gam / (k-s));

		epsv = 0.0; double deltav = 0.0; string compstr;
		computeDPBudget(outputCount, gam, k, t, eps0, epsv, deltav, compstr, lambda+1);


		c += MIN(MAX(outputCount / maxEps, 0.01), 1);
		if(iter >= 100000) { LOG(WARNING) << "Exceeded max iterations."; break; }
		iter++;
	}
	while(epsv > maxEps);

	return;
}

int usage()
{
	cout << "Usage: (1) sgfextract <cfg_file> <gen_out_file> <output_file_prefix> [<k>]" << endl << "\tor" << endl;
	cout << "\t(2) sgfextract <cfg_file> <gen_out_file> <output_file_prefix> [<num_synthetics> <k> <eps0> <tinc>]" << endl << "\tor" << endl;
	cout << "\t(3) sgfextract <cfg_file> <gen_out_file> <output_file_prefix> [<num_synthetics> <lambda> <max_eps>]" << endl << endl;
	cout << "Example: sgfextract workdir/sb.conf workdir/gen/8127.out workdir/extracted_data 50" << endl << "\tor" << endl;
	cout << "\tsgfextract workdir/sb.conf workdir/gen/8127.out workdir/extracted_data 100 500 0.1 25" << endl << "\tor" << endl;
	cout << "\tsgfextract workdir/sb.conf workdir/gen/8127.out workdir/extracted_data 100 60 1.0" << endl;
	return -1;
}

int main(int argc, const char* argv[])
{
	// seed the PRNG
	RNG::getInstance()->defSeedPRNG(); // this seed will be overridden by config

	if(argc < 5)
	{
		return usage();
	}

	RTM* rtm = RTM::getInstance();

	TIC(t0);

	int pid = 0; Config* cfg = NULL; string dataPath = ""; string workdir = ""; string mech = "";
	bool ok = sgfInit(argc, argv, pid, &cfg, dataPath, workdir, mech);
	if(ok == false) { return -1; }

	const string dataFP = argv[2];

	const string outPrefix = argv[3];

	// parse command-line parameters
	u32 outputCount = 0; double lambda = 0.0; double maxEps = 0.0; u32 tinc = 0;
	u32 k = 0; double eps0 = 0.0; bool paramsFromLambda = false;
	bool withDP = true;
	if(argc == 5)
	{
		const string kStr = argv[4];
		int32 kv = parseString<int32>(kStr);
		if(kv <= 0) { cerr << "Invalid k " << kStr << " (must be >0), exiting..." << endl; return -1; }
		k = (u32)kv;

		withDP = false;

		LOG(INFO) << "Extraction parameters: k=" << k << " (plausible deniability only).";
	}
	else if(argc == 7 || argc == 8)
	{
		const string count = argv[4];
		{
			int64 outCount = parseString<int64>(count);
			if(outCount < 0) { cerr << "Invalid output count " << outCount << ", exiting..." << endl; return -1; }
			outputCount = (u64)outCount;
		}

		if(argc == 7)
		{
			paramsFromLambda = true;

			const string lambdaStr = argv[5];
			lambda = parseString<double>(lambdaStr);
			if(lambda <= 0) { cerr << "Invalid lambda " << lambdaStr << " (must be >0), exiting..." << endl; return -1; }

			const string maxEpsStr = argv[6];
			maxEps = parseString<double>(maxEpsStr);
			if(maxEps <= 0) { cerr << "Invalid maxEps " << maxEpsStr << " (must be >0), exiting..." << endl; return -1; }

			LOG(INFO) << "Extraction parameters: lambda=" << lambda << ", and maxEps=" << maxEps << " (DP)";
		}
		else
		{
			VERIFY(argc == 8);

			const string kStr = argv[5];
			int32 kv = parseString<int32>(kStr);
			if(kv <= 0) { cerr << "Invalid k " << kStr << " (must be >0), exiting..." << endl; return -1; }
			k = (u32)kv;

			const string eps0Str = argv[6];
			eps0 = parseString<double>(eps0Str);
			if(eps0 <= 0) { cerr << "Invalid maxEps " << eps0Str << " (must be >0), exiting..." << endl; return -1; }

			LOG(INFO) << "Extraction parameters: count=" << outputCount << ", k=" << k << ", and eps0=" << eps0  << " (DP)";

			const string tincStr = argv[7];
			int32 tincv = parseString<int32>(tincStr);
			if(tincv <= 0 || tincv >= k) { cerr << "Invalid tinc " << tincStr << " (must be >0 and <k), exiting..." << endl; return -1; }
			tinc = (u32)tincv;
		}
	}
	else
	{
		return usage();
	}

	// need omega to check whether we are seedless
	u16 omegaVal = REC_ATTR_COUNT;
	const string& omega = cfg->get<string>(CONFIG_SYNTH_OMEGA);
	if(omega.compare("m") != 0)
	{
		const int32 v = parseString<int32>(omega);
		if(v < 0 || v > REC_ATTR_COUNT) { LOG(ERROR) << "Invalid omega: " << omega << ", exiting..."; return -1; }
		omegaVal = (u16)v;
	}

	bool seedless = false;
	if(mech.compare("seedbased") != 0 || omegaVal == REC_ATTR_COUNT)
	{
		seedless = true;
		LOG(WARNING) << "Model is seedless...";
	}

	SynthDataset* synthData = new SynthDataset(dataFP);
	ok = synthData->load();
	if(ok == false)
	{
		LOG(ERROR) << "Couldn't load synthetic dataset from file " << dataFP << ", exiting...";
		return -1;
	}

	Metadata* metadata = Metadata::getInstance();
	metadata->initialize(dataPath, false);

	const string header = metadata->getHeaderLine();

	const double gam = cfg->get<double>(CONFIG_SYNTH_PARAMS_GAMMA);
	double maxPS = cfg->get<double>(CONFIG_SYNTH_PARAMS_MAXPS);

	if(paramsFromLambda == false && withDP == true)
	{
		if(k < 2){ LOG(ERROR) << "Privacy parameter k is too small to get DP guarantees!"; return -1; }
	}

	const u64 count = synthData->size();
	if(count == 0) { LOG(ERROR) << "Synthetic dataset is empty!"; return -1; }
	if(count >= UINT32_MAX) { LOG(ERROR) << "Synthetic dataset is too large!"; return -1; }

	if(outputCount == 0) { outputCount = count; }

	if(outputCount > count) { LOG(WARNING) << "Synthetic dataset only contains " << count << " records, cannot output more."; outputCount = count; }


	RecordLineWriter* writer = new RecordLineWriter();
	stringstream outprefss("");

	// define OUTPUT_SEEDS if you want to also get the seeds. (Note: publishing the seeds are NOT privacy-preserving.)
#ifdef OUTPUT_SEEDS
	if(seedless == false)
	{
		outprefss.str("");
		outprefss << outPrefix << ".seeds";
		SingleRecordOutputter* seedsOuttputter = new SingleRecordOutputter(writer, outprefss.str(), true);
		seedsOuttputter->outputRaw(header);
	}
#endif

	outprefss.str("");
	outprefss << outPrefix << ".synth";
	SingleRecordOutputter* synthOuttputter = new SingleRecordOutputter(writer, outprefss.str(), true);
	synthOuttputter->outputRaw(header);

	MathUtils* mu = MathUtils::getInstance();
	RNG* rng = RNG::getInstance();

	LOG(INFO) << "Extracting records which pass the privacy test...";

	// create a permutation of the dataset so we extract a random subset
	u32* perm = new u32[count]; if(perm == NULL) { LOG(ERROR) << "Failed to allocate memory."; return -1; }
	for(u32 i=0; i<count; i++) { perm[i] = i; }
	rng->randomPermutation(perm, count);

	double s = 0.0;
	if(paramsFromLambda == true) // compute an appropriate value for k, eps0, etc.
	{
		VERIFY(lambda > 0 && maxEps > 0 && k == 0 && eps0 == 0.0);
		double kv = 0.0;
		computeParamsFromLambda(lambda, maxEps, gam, kv, s, eps0, outputCount);
		k = kv;

		LOG(INFO) << "Parameters computed from lambda: k=" << k << ", eps0=" << eps0;
	}

	size_t passCount = 0;
	for(u32 i=0; i<count; i++)
	{
		if(passCount >= outputCount) { break; }

		u32 idx = perm[i];

		rec seed; rec synth; u64 psc;
		synthData->get(idx, seed, synth, psc);

		bool pass = seedless || psc >= k; // plausible deniability test
		if(seedless == false && withDP == true)
		{
			// differential privacy test
			double noisedpsc = psc + mu->laplaceSample(1.0/eps0);
			pass = noisedpsc >= k;
		}

		if(pass == true) // we passed the test, output this synthetic
		{
			passCount++;
#ifdef OUTPUT_SEEDS
			if(seedless == false) { seedsOuttputter->output(&seed); }
#endif
			synthOuttputter->output(&synth);
		}
	}

	if(passCount == 0) // better luck next time.
	{
		LOG(INFO) << "No records passed the privacy test.\nTweak the privacy parameters (k, eps0) or re-run sgfgen (with larger gamma and max_ps) and try again.";
	}
	else
	{
		LOG(INFO) << "Extracted " << passCount << " synthetics which passed the privacy test.";

		stringstream ss("");
		ss << "Privacy guarantees:\n\t- Plausible Deniability for k=" << k << ", gamma=" << gam << "\n";
		if(withDP == true)
		{
			if(seedless == true) { ss << "\t- Differential Privacy (model is seedless).\n"; }
			else // write out the differential privacy guarantee achieved
			{
				ss << "\t- Differential Privacy:\n";
				if(paramsFromLambda == true)
				{
					const double t = k - s;
					double epsv = 0.0; double deltav = 0.0; string compstr = "";
					computeDPBudget(passCount, gam, k, t, eps0, epsv, deltav, compstr, lambda+1);
					ss << "\t\teps=" << epsv << ", delta=" << deltav << "\t(" << compstr << ")\n";
				}
				else
				{
					for(u32 t=tinc; t<=k-tinc; t+=tinc)
					{
						double epsv = 0.0; double deltav = 0.0; string compstr = "";
						computeDPBudget(passCount, gam, k, t, eps0, epsv, deltav, compstr);
						ss << "\t\t t=" << t << ": eps=" << epsv << ", delta=" << deltav << "\t(" << compstr << ")\n";
					}
				}
			}
		}

		LOG(INFO) << ss.str();
	}

#ifdef OUTPUT_SEEDS
	delete seedsOuttputter;
#endif
	delete synthOuttputter;

	const double et = TOC(t0);
	rtm->dumpToLog();

	LOG(INFO) << "All done in " << ceil(100*et)/100.0 << " seconds.";

	delete writer;
	return 0;
}
