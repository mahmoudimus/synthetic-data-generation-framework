/** Implementation of various generative models. **/

#include "data.h"

#define PRIMARY_BUDGET_NAME "primary"
#define SECONDARY_BUDGET_NAME "secondary"

/** Base class / implementation for the generative model. */
class BaseGenerativeModel : public GenerativeModel<rec>
{
protected:
	typedef struct _cdesc
	{
		u16 attridx;
		set<u16> values;

		string toString()
		{
			stringstream ss(""); ss << attridx << ":";
			u16 i=0;
			foreach_const(set<u16>, values, iter)
			{
				if(i > 0) { ss << ","; }
				ss << *iter; i++;
			}
			return ss.str();
		}
	}
	cdesc, *pcdesc;

protected:
	Config* cfg;
	MathUtils* mu;
	Metadata* metadata;
	RunParameters* rp;
	RTM* rtm;

	vector<u16> order;
	u16 omega;

private:
	u16* statsData;
	u64 statsDataSize;

	map<string, double*> countsMap;
	u64 countsTotalByteSize;

protected:

	typedef struct _privbudget
	{
		double budgetVal;
		double maxQueries;
		double effEps;
	}
	privbudget, *pprivbudget;

	double totalBudget;
	bool advancedComp;
	bool geomNoise;

	map<string, privbudget> budgets;


public:
	BaseGenerativeModel(vector<prec>& data)
		: cfg(Config::getInstance()), mu(MathUtils::getInstance()), metadata(Metadata::getInstance()),
		  rp(RunParameters::getInstance()), rtm(RTM::getInstance()),
		  omega(REC_ATTR_COUNT), countsTotalByteSize(0), totalBudget(0.0), advancedComp(false), geomNoise(false)
	{
		const string& omegaStr = cfg->get<string>(CONFIG_SYNTH_OMEGA);
		if(omegaStr.compare("m") != 0)
		{
			int32 tmpOmega = parseString<int32>(omegaStr);
			VERIFY(tmpOmega >= 0 && tmpOmega <= REC_ATTR_COUNT);
			omega = (u16)tmpOmega;
		}

		metadata->getOrder(order); VERIFY(order.size() == REC_ATTR_COUNT);
		const string graphOrder = describeVector(order);

		if(omega < REC_ATTR_COUNT) // adjust order based on omega
		{
			const size_t firstResampleIdx = REC_ATTR_COUNT - omega;

			vector<u16> newOrder;
			for(size_t i=0; i< order.size(); i++)
			{
				const u16 attrIdx = order[i];
				if(i >= firstResampleIdx) { newOrder.push_back(attrIdx); }
			}
			order.clear();
			order.insert(order.begin(), newOrder.begin(), newOrder.end());
		}

		LOG(INFO) << "(Graph) order: " << graphOrder << "; effective order: " << describeVector(order) << ", omega: " << (u32)omega;

		const pbudgetmeta bm = metadata->getBudgetMetadata("stats");
		totalBudget = bm->weps;

		LOG(INFO) << "[Generative model privacy budget] " << bm->name << ", budget: " << totalBudget;

		const string& ndist = cfg->get<string>(CONFIG_SYNTH_PRIVACY_NOISE_DIST);
		if(ndist.compare("no") == 0 || ndist.compare("none") == 0) { totalBudget = 0.0; }
		else if(ndist.compare("lap") == 0) { ; }
		else if(ndist.compare("geom") == 0) { geomNoise = true; }
		else { LOG(ERROR) << "Unrecognized noise dist: " << ndist << ", exiting..."; exit(-1); }

		const string& ncomp = cfg->get<string>(CONFIG_SYNTH_PRIVACY_NOISE_COMP);

		advancedComp = false;
		if(ncomp.compare("seq") == 0 || ncomp.compare("def") == 0) { ; }
		else if(ncomp.find("adv") == 0) { advancedComp = true; }
		else { LOG(ERROR) << "Unrecognized noise composition: " << ncomp << ", exiting..."; exit(-1); }

		statsDataSize = data.size();
		const size_t tsz = statsDataSize * REC_ATTR_COUNT;
		statsData = new u16[tsz]; FILL_ARRAY(statsData, tsz, -1);
		for(u32 i=0; i<statsDataSize; i++)
		{
			prec rec = data[i];
			for(u16 j=0; j<REC_ATTR_COUNT; j++)
			{
				size_t idx = GET_INDEX(i, j, REC_ATTR_COUNT);
				statsData[idx] = rec->vals[j];
			}
		}
		// sanity check
		for(size_t i=0; i<tsz; i++) { const u16 v = statsData[i]; VERIFY(v != INVALID_RECORD_VAL && v <= MAX_REC_VALUE); }

		LOG(INFO) << "Generative model started up with " << statsDataSize << " records from 'stats' dataset";
	}
	virtual ~BaseGenerativeModel()
	{
		if(statsData != NULL) { delete[] statsData; }
		pair_foreach(map<string, double*>, countsMap, iter)
		{
			double* v = iter->second;
			if(v != NULL) { delete[] v; }
		}
		countsMap.clear();
	}

	virtual prec propose(const rec* seed) = 0;
	virtual double pdf(const rec* seed, const rec* fake) = 0;

	virtual bool lnpdf() { return true; }

	virtual bool isSeedless() { return false; }

protected:

	virtual void setupBudgets() = 0; // pure virtual

	void showBudgets(void)
	{
		const string& ndist = cfg->get<string>(CONFIG_SYNTH_PRIVACY_NOISE_DIST);
		const string& ncomp = cfg->get<string>(CONFIG_SYNTH_PRIVACY_NOISE_COMP);

		LOG(INFO) << "[DP] ndist: " << ndist << ", ncomp: " << ncomp << ", i.e. (totalBudget: " << totalBudget << ")";

		if(totalBudget > 0) { VERIFY(budgets.empty() == false); }

		if(budgets.empty() == true) { LOG(INFO) << "[DP] no budgets."; }

		double budgetSum = 0.0;
		pair_foreach_const(map<string, privbudget>, budgets, iter)
		{
			const string& name = iter->first;
			const privbudget& budget = iter->second;
			LOG(INFO) << "[DP] budget: " << name << ", eps: " << budget.budgetVal
					<< ", max queries: " << budget.maxQueries << ", effEps: " << budget.effEps;
			budgetSum += budget.budgetVal;
		}

		checkBudget(budgetSum);
	}

	virtual void checkBudget(double budgetSum)
	{
		VERIFY(budgetSum <= totalBudget + EPSILON);
		const double absDiff = totalBudget - budgetSum;
		if(absDiff >= 0.1) { LOG(WARNING) << "[!!DP!! -- Warning] underused privacy budget: " << budgetSum << " / " << totalBudget; }
	}

	void getOrder(vector<u16>& orderToUse)
	{
		orderToUse.clear();
		orderToUse.insert(orderToUse.begin(), order.begin(), order.end());

		const bool verb = rp->equalDebugLevel(RunParameters::DEBUG_VERBOSE_LEVEL);
		if (verb) { LOG(TRACE) << "Using order: " << describeVector(orderToUse);}

		VERIFY(orderToUse.size() == omega);
		set<u16> tmpOrderSet(orderToUse.begin(), orderToUse.end());
		VERIFY(orderToUse.size() == omega);
	}

	void getCDS(const rec* rec, const set<u16>& fs, vector<cdesc>& cds, bool verb = false)
	{
		// we must not clear 'cds' here
		foreach_const(set<u16>, fs, iter)
		{
			cdesc c; c.attridx = *iter; c.values.clear();
			const pattrgrp g = metadata->getAttrGrouping(c.attridx);

			const u16 rv = rec->vals[c.attridx];
			const u16 grpidx = g->iv[rv];
			map<u16, set<u16>>::iterator iterG = MAP_LOOKUP(g->gmap, grpidx);
			VERIFY(iterG != g->gmap.end());

			set<u16>& grpset = iterG->second;
			c.values.insert(grpset.begin(), grpset.end());
			VERIFY(SET_CONTAINS(c.values, rv) == true);

			if(verb == true) { LOG(TRACE) << "For attr " << c.attridx << " rec; rv: " << dec << rv << ", grpidx: " << grpidx << ", grpset: " << describeSet<u16>(grpset); }

			cds.push_back(c);
		}
	}

	// this is just a sanity check
	void checkCDS(const u16 attrIdx, const vector<cdesc>& cds)
	{
		// sanity check: make sure each attr appears at most once (and attrIdx doesn't appear)
		set<u16> tmpCheck;
		foreach_const(vector<cdesc>, cds, iterCDS)
		{
			const cdesc& c = *iterCDS;
			VERIFY(c.attridx != attrIdx);
			tmpCheck.insert(c.attridx);
		}
		VERIFY(tmpCheck.size() == cds.size()); // if an attribute has multiple cdescs, then this check will fail!
	}

	double* count(u16 attrIdx, vector<cdesc>& cds)
	{
		TIC(st)

		double* c = NULL;
		stringstream ss(""); ss << attrIdx << "_";
		{
			u32 l=0;
			foreach_const(vector<cdesc>, cds, iter)
			{
				if(l > 0) { ss << ";"; } l++;
				cdesc cd = *iter;
				ss << cd.toString();
			}
		}
		const string key = ss.str();

		pattrmeta am = metadata->getAttrMetadata(attrIdx);
		const u16 numVals = am->vals;

		const double dirichletHyper = cfg->get<double>(CONFIG_SYNTH_DIRICHLET_HYPER);
		const double fillVal = dirichletHyper / numVals;

		map<string, double*>::const_iterator iter = MAP_LOOKUP(countsMap, key);
		if(iter == countsMap.end())
		{
			countsTotalByteSize += numVals * sizeof(double) + sizeof(double*);

			c = new double[numVals]; if(c == NULL) { LOG(ERROR) << "Failed to allocate memory."; } VERIFY(c != NULL);
			FILL_ARRAY(c, numVals, fillVal);
			for(u64 i=0; i<statsDataSize; i++)
			{
				bool allMatch = true;
				foreach_const(vector<cdesc>, cds, iter)
				{
					const cdesc cd = *iter;
					const u16 j = cd.attridx;

					const u16 val = statsData[GET_INDEX(i, j, REC_ATTR_COUNT)];
					if(SET_CONTAINS(cd.values, val) == false) { allMatch = false; break; }
				}

				if(allMatch == true)
				{
					const u16 attrVal = statsData[GET_INDEX(i, attrIdx, REC_ATTR_COUNT)];
					c[attrVal] += 1.0;
				}
			}

			addNoise(c, numVals, key, fillVal, PRIMARY_BUDGET_NAME);

			countsMap[key] = c;

			double etn = TOC(st);
			rtm->add("Imputation::count-new-Elapsed", etn);
		}
		else
		{
			c = iter->second; // retrieve previously computed vector
		}

		const double et = TOC(st);
		rtm->add("Imputation::count-Elapsed", et);

		return c;
	}

	void calculateBudget(privbudget& budget)
	{
		if(budget.budgetVal <= 0) { budget.effEps = 0.0; return; }

		double seqEps = budget.budgetVal / budget.maxQueries;
		double effEps = seqEps;
		if(advancedComp == true)
		{
			// rule of thumbs: advanced composition is only better than sequential composition for 64 or more queries (for final epsilon ~= 1).
			const double lambda = cfg->get<double>(CONFIG_SYNTH_PRIVACY_GENMODEL_LAMBDA);	VERIFY(lambda > 0.0);
			const double invlndelta = lambda/log2(exp(1));
			const double k = budget.maxQueries; // number of queries (has nothing to do with the k of plausible deniability)
			const double sq = sqrt(2 * k * invlndelta);

			// since we can't analytically express effEps based on the adv. comp. theorem,
			// let's use the approximation 'eps1 * (exp(eps1) - 1) ≃ pow(eps1, 2.0)', then once done
			// we'll recalculate eps1 based on effEps; if the difference is too large try again with a smaller eps
			double eps = budget.budgetVal; double targetEps = 0.0; double decv = 0.001;
			do
			{
				VERIFY(eps > 0);
				effEps = (1.0 / (2*k)) * (-sq + sqrt(sq*sq + 4*k*eps)); // quadratic formula assuming eps * (exp(eps) - 1) ≃ pow(eps, 2.0)
				targetEps = effEps * sq + k * effEps * (exp(effEps) - 1.0);
				eps -= decv;
			}
			while(targetEps > budget.budgetVal);

			if(effEps < seqEps) // automatically use the best composition strategy
			{
				LOG(WARNING) << "[!!DP!!] Adv. comp yielded a smaller effEps (" << effEps
											<< ") than required for seq. comp (" << seqEps << "), using the latter.";
				effEps = seqEps;
			}
		}
		budget.effEps = effEps;

		VERIFY(effEps > 0);

		LOG(INFO) << "[DP] effEps -> " << effEps << " (max queries: " << budget.maxQueries
				<< ", eps1: " << budget.budgetVal << ", advComp: " << advancedComp << ")";
	}

	double getNoiseValue(const string& budgetName)
	{
		if(totalBudget <= 0.0) { return 0.0; }
		VERIFY(budgets.empty() == false); // budget has not been setup

		map<string, privbudget>::iterator iter = MAP_LOOKUP(budgets, budgetName);
		VERIFY(iter != budgets.end());

		privbudget budget = iter->second;
		if(budget.effEps <= 0) { return 0.0; }

		const double effEps = budget.effEps;
		const double b = 1.0 / effEps;

		double ret = 0.0;
		if(geomNoise == true)
		{
			// we add noise with alpha = e^(-effEps) so that we get alpha - DP (i.e., eps-DP)
			const double alpha = exp(-effEps);
			ret = mu->geomDPSample(alpha);
		}
		else { ret = mu->laplaceSample(b); }

		return ret;
	}

	void addNoise(double* c, const u16 numVals, const string key, const double fillVal, const string& budgetName)
	{
		const bool verb = rp->equalDebugLevel(RunParameters::DEBUG_VERBOSE_LEVEL);
		const bool seededNoise = cfg->get<bool>(CONFIG_SYNTH_SEEDED_NOISE);

		if(seededNoise == true)
		{
			// note: in most cases we want seeded noise (one exception is for model error measurements)
			const u64 h = hashOf(key);
			mu->setRNGSeed(h);

			if(verb == true) { LOG(TRACE) << "Setting RNG seed to " << hex << h << dec << ", key: " << key; }
			//// -- using deterministic seed (based on key)
		}

		if(totalBudget > 0)
		{
			// add DP noise
			for(u16 i=0; i<numVals; i++)
			{
				const double noise = getNoiseValue(budgetName);

				c[i] += noise;
				if(c[i] < fillVal) { c[i] = fillVal; }
			}
		}

		if(seededNoise == true)
		{
			//// --- resetting seed
			mu->resetRNGSeed();
			if(verb == true) { LOG(TRACE) << "Resetting RNG seed."; }
		}
	}

	virtual void initialize()
	{
		// take care of privacy budgeting
		setupBudgets();
		showBudgets();
	}

	virtual void shutdown()
	{
		u32 memMB = (u32)ceil(countsTotalByteSize / (1024*1024));
		LOG(INFO) << "[Performance] Memory usage for the Dirichlet params: " << memMB << " MB";
	}
};

class MarginalsGenerativeModel : public BaseGenerativeModel
{
private:
	MathUtils* mu;
	Metadata* metadata;
	RunParameters* rp;

	double** vcs;
	bool uniform;

public:
	MarginalsGenerativeModel(vector<prec>& data, bool unif = false)
		: BaseGenerativeModel(data),
		  mu(MathUtils::getInstance()), metadata(Metadata::getInstance()), rp(RunParameters::getInstance()),
		  uniform(unif)
	{
		vcs = new double*[REC_ATTR_COUNT];
		FILL_ARRAY(vcs, REC_ATTR_COUNT, NULL);

		const u64 sz = data.size();
		for(u32 i=0; i<sz; i++)
		{
			prec rec = data[i];
			for(u16 j=0; j<REC_ATTR_COUNT; j++)
			{
				pattrmeta am = metadata->getAttrMetadata(j);
				const u16 numVals = am->vals;

				double* vc = vcs[j];
				if(vc == NULL)
				{
					vc = new double[numVals];
					const double dirichletHyper = cfg->get<double>(CONFIG_SYNTH_DIRICHLET_HYPER);
					const double fillVal = dirichletHyper / numVals;
					FILL_ARRAY(vc, numVals, fillVal);
					vcs[j] = vc;
				}

				u16 val = rec->vals[j];
				vc[val] += 1.0;
			}
		}

		LOG(INFO) << "Baseline generative model started up with " << sz << " records from 'stats' dataset";
	}

	~MarginalsGenerativeModel()
	{
		if(vcs != NULL)
		{
			for(u16 j=0; j<REC_ATTR_COUNT; j++)
			{
				double* vc = vcs[j];
				delete[] vc;
			}
			delete[] vcs;
		}
	};

	virtual prec propose(const rec* seed)
	{
		VERIFY(seed == NULL);
		prec fake = new rec; allocateRecord(fake, INVALID_RECORD_VAL);

		for(u16 j=0; j<REC_ATTR_COUNT; j++) // just iterates over each attribute, each time sample from marginals
		{
			pattrmeta am = metadata->getAttrMetadata(j);
			const u16 numVals = am->vals;

			double* vc = vcs[j]; VERIFY(vc != NULL);

			u16 fv = INVALID_RECORD_VAL;
			if(uniform == true) { fv = mu->sampleFromVector(vc, numVals); }
			else { fv = (u16)mu->sampleDirichletMultinomialFromVector(vc, numVals); } // sample from marginals
			fake->vals[j] = fv;
		}

		double psf = pdf(seed, fake);
		LOG(TRACE) << "fake: " << getRecordDesc(fake) << ", ln(prob): " << psf;
		VERIFY(psf < 0.0 && std::isnan(psf) == false);

		return fake;
	}

	virtual double pdf(const rec* seed, const rec* fake)
	{
		VERIFY(seed == NULL);
		double ret = 0.0;

		for(u16 j=0; j<REC_ATTR_COUNT; j++) // iterate over each attribute, compute probability of the obtained sample
		{
			pattrmeta am = metadata->getAttrMetadata(j);
			const u16 numVals = am->vals;

			const u16 fv = fake->vals[j];

			double* vc = vcs[j]; VERIFY(vc != NULL);

			double p = 0.0;
			if(uniform == true) { p = vc[fv]; }
			else { p = mu->dirichletMultinomialSingleTrialPMF(vc, numVals, fv); }

			ret += log(p); // log(product_j prob_j) -> sum_j log(prob_j)
		}

		return ret;
	}

	virtual bool isSeedless() { return true; }

protected:
	virtual void setupBudgets()
	{
		VERIFY(budgets.empty() == true);
		if(totalBudget <= 0) { return; }

		privbudget primary;
		primary.budgetVal = totalBudget;
		primary.maxQueries = REC_ATTR_COUNT;
		calculateBudget(primary);

		budgets[PRIMARY_BUDGET_NAME] = primary;
	}

	virtual void initialize()
	{
		BaseGenerativeModel::initialize();

		// normalize
		for(u16 j=0; j<REC_ATTR_COUNT; j++)
		{
			pattrmeta am = metadata->getAttrMetadata(j);
			const u16 numVals = am->vals;

			double* vc = vcs[j]; VERIFY(vc != NULL);

			if(uniform == true)
			{
				FILL_ARRAY(vc, numVals, 1.0);
				mu->normalize(vc, numVals); // only normalize for uniform
			}
			else
			{
				stringstream ss(""); ss << "i0_" << am->name;
				const string& key = ss.str();

				const double dirichletHyper = cfg->get<double>(CONFIG_SYNTH_DIRICHLET_HYPER);
				const double fillVal = dirichletHyper / numVals;
				addNoise(vc, numVals, key, fillVal, PRIMARY_BUDGET_NAME); // add the noise -> this model will also guarantee DP
			}

			LOG(INFO) << "Sampling vc for attr " << j << " -> " << describeArray<double>(vc, numVals);
		}
	}
};


class SeedBasedGenerativeModel : public BaseGenerativeModel
{
public:
	SeedBasedGenerativeModel(vector<prec>& data) : BaseGenerativeModel(data)
	{
		VERIFY(cfg->get<bool>(CONFIG_SYNTH_SEEDED_NOISE) == true); // must be true!

		LOG(INFO) << "Seed-based generative model started up with omega: " << omega;

		if(omega == REC_ATTR_COUNT)
		{
			LOG(WARNING) << "[Warning] omega is set to " << REC_ATTR_COUNT << " -> this is seedless!";
		}
	}

	~SeedBasedGenerativeModel() { }

	virtual prec propose(const rec* seed)
	{
		TIC(st);

		const bool verb = rp->equalDebugLevel(RunParameters::DEBUG_VERBOSE_LEVEL);

		// start with the fake filled with 'INVALID_RECORD_VAL'
		// (attributes not imputed will be set to the value of the seed)
		prec fake = new rec; allocateRecord(fake, INVALID_RECORD_VAL);

		if(omega < REC_ATTR_COUNT)
		{
			VERIFY(seed != NULL); // when omega < REC_ATTR_COUNT, we are not seedless, so seed can't be NULL!

			// due to omega we will not resample all attributes, so let's copy over those from the seed (to the fake)
			set<u16> orderSet(order.begin(), order.end());
			for(u16 attrIdx = 0; attrIdx < REC_ATTR_COUNT; attrIdx++)
			{
				if(SET_CONTAINS(orderSet, attrIdx) == false)
				{
					// the ones which are not in the order are not getting resampled!
					fake->vals[attrIdx] = seed->vals[attrIdx];
				}
			}
		}

		const string seedDesc = ((seed == NULL) ? "NULL" : getRecordDesc(seed));
		LOG(TRACE) << "propose() called on seed: " << seedDesc;

		const u16 orderSize = omega;

		vector<u16> orderToUse; getOrder(orderToUse); // getOrder(false, fake, orderToUse);
		VERIFY(orderToUse.size() == orderSize);

		set<u16> resampledAttrIdx;

		// do imputation in order
		foreach_const(vector<u16>, orderToUse, iterAttrIdx)
		{
			const u16 attrIdx = *iterAttrIdx;

			u16 numVals = 0; double* vc = NULL;
			getVC(fake, attrIdx, resampledAttrIdx, &vc, &numVals); VERIFY(vc != NULL && numVals > 0);

			u16 validx = (u16)mu->sampleDirichletMultinomialFromVector(vc, numVals); VERIFY(validx < numVals);

			fake->vals[attrIdx] = validx; // set the value of the fake

			if(verb == true) { LOG(TRACE) << "\tvc: " << describeArray<double>(vc, numVals) << ", sampled: " << validx; }

			resampledAttrIdx.insert(attrIdx);
		}
		VERIFY(resampledAttrIdx.size() == orderSize);

		// sanity check
		const double psf = pdf(seed, fake);
		LOG(TRACE) << "seed: " << seedDesc << " -> fake: " << getRecordDesc(fake) << ", ln(prob): " << psf;
		VERIFY(psf <= 0.0 && std::isnan(psf) == false);

		const double et = TOC(st);
		rtm->add("SeedBased::proposed-Elapsed", et);

		return fake;
	}

	virtual double pdf(const rec* seed, const rec* fake)
	{
		TIC(st);

		double ret = 0.0;

		const u16 orderSize = omega;

		if(omega < REC_ATTR_COUNT)
		{
			VERIFY(seed != NULL); // when omega < REC_ATTR_COUNT, we are not seedless, so seed can't be NULL!

			// if the fake doesn't match the seed on the non-resampled attribute, then the pdf is 0
			set<u16> orderSet(order.begin(), order.end());
			for(u16 attrIdx = 0; attrIdx < REC_ATTR_COUNT; attrIdx++)
			{
				// non-resampled attributes are the ones *not* in the order
				if(SET_CONTAINS(orderSet, attrIdx) == false)
				{
					if(fake->vals[attrIdx] != seed->vals[attrIdx])
					{
						const double et = TOC(st);
						rtm->add("SeedBased::pdf-Elapsed", et);

						return log(0.0); // this return -inf (which is what we want here)
					}
				}
			}
		}

		vector<u16> orderToUse; getOrder(orderToUse); // getOrder(false, fake, orderToUse);
		VERIFY(orderToUse.size() == orderSize);

		// do resampling in order
		set<u16> resampledAttrIdx;
		foreach_const(vector<u16>, orderToUse, iterAttrIdx)
		{
			const u16 attrIdx = *iterAttrIdx;

			u16 numVals = 0; double* vc = NULL;
			getVC(fake, attrIdx, resampledAttrIdx, &vc, &numVals); VERIFY(vc != NULL && numVals > 0);

			u16 fakeVal = fake->vals[attrIdx];
			const double ptmp = mu->dirichletMultinomialSingleTrialPMF(vc, numVals, fakeVal);

			double logp = log(ptmp);

			ret += logp;
			resampledAttrIdx.insert(attrIdx);
		}
		VERIFY(resampledAttrIdx.size() == orderSize);

		const double et = TOC(st);
		rtm->add("SeedBased::pdf-Elapsed", et);

		return ret;
	}

	virtual bool isSeedless() { return omega == REC_ATTR_COUNT; }

protected:

	void getVC(const rec* fake, const u16 attrIdx, set<u16>& resampledAttrIdx, double** outVC, u16* outNumVals)
	{
		const bool verb = rp->equalDebugLevel(RunParameters::DEBUG_VERBOSE_LEVEL);

		const pbfs bfs = metadata->getbfs(attrIdx);
		set<u16> fs; fs.insert(bfs->attridx.begin(), bfs->attridx.end());

		VERIFY(SET_CONTAINS(fs, attrIdx) == false); // sanity check

		set<u16> copiedAttrIdx;
		set_difference(fs.begin(), fs.end(), resampledAttrIdx.begin(), resampledAttrIdx.end(), inserter(copiedAttrIdx, copiedAttrIdx.begin()));
		set<u16> newlyAvailAttrIdx;
		set_intersection(fs.begin(), fs.end(), resampledAttrIdx.begin(), resampledAttrIdx.end(), inserter(newlyAvailAttrIdx, newlyAvailAttrIdx.begin()));

		// sanity check
		VERIFY(fs.size() == copiedAttrIdx.size() + newlyAvailAttrIdx.size());
		foreach(set<u16>, fs, fsIterSC)
		{
			const u16 fsAttrIdx = *fsIterSC; VERIFY(fsAttrIdx < REC_ATTR_COUNT);
			const u16 v = fake->vals[fsAttrIdx];
			if(v == INVALID_RECORD_VAL) // if this happens something is wrong.
			{ LOG(ERROR) << "Inconsistency between the dependency DAG and the provider topological sorting order!"; }
			VERIFY(v != INVALID_RECORD_VAL && v <= MAX_REC_VALUE);
		}

		// do the imputation
		pattrmeta am = metadata->getAttrMetadata(attrIdx);
		const u16 numVals = am->vals;
		double* vc = NULL;

		if(verb == true)
		{
			LOG(TRACE) << "\tImputing attr: " << attrIdx << " (" << numVals << " vals), using fs: " << describeSet<u16>(fs)
			           << " [copiedAttrIdx: " << describeSet<u16>(copiedAttrIdx) << ", newlyAvailAttrIdx: " << describeSet<u16>(newlyAvailAttrIdx) << "]";
		}

		// when both copiedAttrIdx and newlyAvailAttrIdx are empty, this will just get the marginals (as it should);
		imputationVecor(attrIdx, fake, fake, copiedAttrIdx, newlyAvailAttrIdx, &vc); // give the fake as both seed & fake (since we copied the initial seed attrs)
		VERIFY(vc != NULL);

		*outVC = vc;
		*outNumVals = numVals;
	}

	void imputationVecor(const u16 attrIdx, const rec* seed, const rec* fake,
						const set<u16>& seedAttrIdx, const set<u16>& fakeAttrIdx, double** pvc)
	{
		vector<cdesc> cds;

		getCDS(seed, seedAttrIdx, cds); // for the seed

		getCDS(fake, fakeAttrIdx, cds); // for the fake

		checkCDS(attrIdx, cds); // sanity check

		// sort the cds (using lambdas)
		sort(cds.begin(), cds.end(), [](const cdesc& left, const cdesc& right)
									{ return left.attridx < right.attridx; });

		checkCDS(attrIdx, cds); // sanity check

		*pvc = count(attrIdx, cds);
	}

	virtual void setupBudgets()
	{
		if(totalBudget <= 0) { return; }

		privbudget budget;
		budget.budgetVal = totalBudget;
		budget.maxQueries = REC_ATTR_COUNT;
		calculateBudget(budget);

		VERIFY(budgets.empty() == true);
		budgets[PRIMARY_BUDGET_NAME] = budget;
	}
};
