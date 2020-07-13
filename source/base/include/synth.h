/** Implementation of a synthesizer class according to the plausible deniability mechanism. **/
#ifndef SGFBASE_SYNTH_H_
#define SGFBASE_SYNTH_H_

/** Parameters for the synthesis. */
typedef struct _synthparams
{
	double gam;

	size_t fakesPerSeed;
	size_t count;

	double runtime;

	bool randomPSOrder;
	size_t maxPS;
	size_t maxCheckPS;
}
synthparams, *psynthparams;

/** Properties of synthesized records. */
typedef struct _synthprops
{
	psynthparams params;

	double psf;
	bool lnPDF;
	int32 ecidx; // partition / equivalence class idx

	double psCount;
}
synthprops, *psynthprops;

typedef struct _psinfo
{
	size_t checked;
	size_t found;
}
psinfo, *ppsinfo;

/** The synthesizer class. */
template <class R>
class Synthesizer
{
private:
	MathUtils* mu;
	RNG* rng;
	RTM* rtm;

	GenerativeModel<R>* gen;

	vector<R*> dataset;
	vector<R*> recs;

	synthparams params;

public:
	Synthesizer(vector<R*> data, GenerativeModel<R>* g)
	{
		mu = MathUtils::getInstance();
		rng = RNG::getInstance();
		rtm = RTM::getInstance();

		gen = g;

		dataset = data;
		if(dataset.size() >= INT32_MAX) { LOG(ERROR) << "Input dataset is too large!"; exit(-1); }
	}
	virtual ~Synthesizer() { recs.clear(); }

	bool run(const synthparams* params, Outputter<R>* outputter);


protected:
	virtual bool validateParams();
	virtual void logParams();

	int32 probClass(const double p, const double gamma, const bool lnPDF);
	void psCount(const R* fake, const double psf, const bool lnPDF, u32* perm, set<size_t>& psIdxSet, ppsinfo psInfo);
	R* pickSeed(size_t* outIdx);
	void partition();
};

template <class R>
bool Synthesizer<R>::validateParams()
{
	if(params.fakesPerSeed <= 0) { LOG(ERROR) << "[Error] Invalid Synthesizer parameter (fps: " << params.fakesPerSeed << ")."; return false; }
	if(params.count <= 0) { LOG(ERROR) << "[Error] Invalid Synthesizer parameter (count: " << params.count << ")."; return false; }
	if(params.runtime <= 0) { LOG(ERROR) << "[Error] Invalid Synthesizer parameter (runtime: " << params.runtime << ")."; return false; }

	if(params.gam <= 1) { LOG(ERROR) << "[Error] Invalid Synthesizer parameter (gamma: " << params.gam << " -- must be > 1)."; return false; }

	return true;
}

template <class R>
void Synthesizer<R>::logParams()
{
	LOG(INFO) << "[Params -- Generation] count: " << params.count << ", runtime: " << params.runtime << ", fps: " << params.fakesPerSeed;
	LOG(INFO) << "[Params -- Privacy Test] gamma: " << params.gam;
	LOG(INFO) << "[Params -- Plausible Seeds] maxCheck: " << params.maxCheckPS << "; maxToSearchFor: " << params.maxPS << ", random order: " << params.randomPSOrder;
}

/** Do the synthesis using the given parameters, use outputter to output the candidates. */
template <class R>
bool Synthesizer<R>::run(const synthparams* pparams, Outputter<R>* outputter)
{
	VERIFY(outputter != NULL && pparams != NULL);
	params = *pparams; // set params

	if(validateParams() == false) { return false; }

	LOG(INFO) << "Starting Synthesizer..."; TIC(st);

	partition();

	params.maxCheckPS = MIN(params.maxCheckPS, recs.size());
	if(params.maxCheckPS > 0) { params.maxPS = MIN(params.maxCheckPS, params.maxPS); }

	logParams();

	gen->initialize(); // initialize the generative model

	size_t effc = params.count;

	size_t c = 0; bool lnPDF = gen->lnpdf();
	TIC(startc);

	const bool seedless = gen->isSeedless(); // determine whether the model is seedless

	u32* perm = NULL;
	if(seedless == false)
	{
		// create array for permutation, we will use this in psCount
		perm = new u32[recs.size()]; if(perm == NULL) { LOG(ERROR) << "Failed to allocate memory."; exit(-1); }
	}

	while(c < effc)
	{
		size_t seedIdx = 0;
		const R* seed = pickSeed(&seedIdx); LOG(TRACE) << "Picked seedIdx: " << seedIdx; // pick a seed record

		for(size_t i=0; i<params.fakesPerSeed; i++) // try to synthesize 'params.fakesPerSeed' from this seed
		{
			TIC(stiter);

			const R* seedPtr = (seedless == true) ? NULL : seed; // if seedless -> set the seed to NULL to avoid it being used in any way.

			// get a synthetic candidate (fake) from the generative model using the given seed.
			R* fake = gen->propose(seedPtr); LOG(TRACE) << "Generated fake (addr: 0x" << hex << (u64)fake << dec << ")";
			fake->fi.seedIdx = seed->idx; fake->idx = c;

			size_t psc = 0; size_t ecidx = 0;
			double psf = 0.0;

			synthprops props;
			if(seedless == true) // if seedless, we won't look for plausible seeds
			{
				psf = lnPDF ? 0.0 : 1.0;
				psc = dataset.size();
				ecidx = -1;
			}
			else // model is NOT seedless, need to count plausible seeds
			{
				// get the probability of this fake being generated from this seed
				psf = gen->pdf(seed, fake); LOG(TRACE) << "Calculated psf: " << psf << " (lnPDF: " << lnPDF << ")";
				VERIFY((lnPDF == true && psf <= 0.0) || psf > 0.0);
				ecidx = probClass(psf, params.gam, lnPDF); // calculate the class / partition number

				LOG(TRACE) << "Counting plausible seeds...";

				set<size_t> foundPS;
				psinfo psInfo; psInfo.checked = 0; psInfo.found = 0;

				// count all plausible seeds
				psCount(fake, psf, lnPDF, perm, foundPS, &psInfo);

				psc = foundPS.size();

				double perc = 100.0 * ((double)psInfo.found / psInfo.checked);
				LOG(TRACE) << "Found: " << psc << " plausible seeds (maxPS: " << params.maxPS << ", checked: " << psInfo.checked
							<< " -- " << fixed << setprecision(2) << perc << "%)";
			}

			// set the properties
			props.params = &params;
			props.psf = psf;
			props.ecidx = ecidx;
			props.lnPDF = lnPDF;
			props.psCount = psc;

			fake->fi.props = props;

			// output the candidate
			outputter->output(seed, fake);

			// free the fake's memory
			freeRecord(fake); delete fake; fake = NULL;

			c++;

			const double etiter = TOC(stiter);
			rtm->add("Synthesizer::RunIter-Elapsed", etiter);
		}

		const double elapsed = TOC(startc);
		if(elapsed > params.runtime)
		{
			LOG(INFO) << "Synthesizer exiting before exceeding allowed time (elapsed: " << elapsed << " seconds).";
			break;
		}
	}

	if(perm != NULL) { delete[] perm; perm = NULL; } // cleanup

	gen->shutdown();

	if(c >= effc)
	{
		LOG(INFO) << "Synthesizer exited after producing the required number of fakes (" << c << ").";
	}

	const double et = TOC(st);
	rtm->add("Synthesizer::Run-Elapsed", et);

	return true;
}

/** Calculate the probability class / partition number.*/
template <class R>
inline int32 Synthesizer<R>::probClass(const double p, const double gam, const bool lnPDF)
{
	if(p == 0.0)
	{
		if(lnPDF == false) { return INT_MIN; }
	}
	else if(p == -INFINITY) // check for isinf(p) == -1
	{
		if(lnPDF == true) { return INT_MIN; }
	}
	return (lnPDF == true) ? ceil(-p/log(gam)) : ceil(-log(p)/log(gam));
}

/** count the number of plausible seeds. */
template <class R>
void Synthesizer<R>::psCount(const R* fake, const double psf, const bool lnPDF, u32* perm, set<size_t>& psIdxSet, ppsinfo psInfo)
{
	TIC(st);

	const double gam = params.gam;
	const double maxPS = params.maxPS;
	const double maxPSToCheck = params.maxCheckPS;

	const int32 psfclass = probClass(psf, gam, lnPDF);
	VERIFY(psfclass >= 0); // class of the seed must be >= 0 since the probability must be > 0

	const size_t sz = recs.size(); VERIFY(sz > 0 && sz <= INT32_MAX);
	set<size_t> checked;


	VERIFY(perm != NULL);
	for(size_t i=0; i<sz; i++) { perm[i] = i; } // initialize the permutation array
	if(params.randomPSOrder == true) { rng->randomPermutation(perm, sz); } // only permute if necessary

	size_t i = 0;
	for(i=0; i<sz; i++)
	{
		if(maxPS > 0 && psIdxSet.size() >= maxPS) { break; }
		if(maxPSToCheck > 0 && i >= maxPSToCheck) { break; }

		size_t idx = perm[i];

		const R* r = recs[idx];
		const double prf = gen->pdf(r, fake);

		const int32 prfclass = probClass(prf, gam, lnPDF); // class can be < 0 but then it can't be equal to psfclass
		if(prfclass == psfclass) { psIdxSet.insert(idx); } // add it to the plausible seed if its in the same class
	}
	psInfo->checked = i;
	psInfo->found = psIdxSet.size();

	const double et = TOC(st);
	rtm->add("Synthesizer::PSCount-Elapsed", et);
}

/** Pick a uniformly random seed record. */
template <class R>
R* Synthesizer<R>::pickSeed(size_t* outIdx)
{
	size_t idx = (size_t)rng->uniformRandomULLBetween(0, recs.size()-1);
	*outIdx = idx;
	return recs[idx];
}

/** Partition the dataset. */
template <class R>
void Synthesizer<R>::partition()
{
	VERIFY(dataset.size() > 0);
	LOG(INFO) << "Partitioning dataset...";

	recs.clear();
	recs = dataset; // just copy the whole thing (take all the records).

	LOG(INFO) << "Done (recs: " << recs.size() << ").";
}
#endif /* SGFBASE_SYNTH_H_ */
