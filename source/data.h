/** Definitions and implementation of data manipulation classes and functions.
 *  This defines the structure of input data files, data records, and associated metatada. **/

/* hardlimit */
#define MAX_REC_ATTR_COUNT (S16_MAX)

/* softlimit (hardlimit is technically U16_MAX - 1, but this provides some margin) */
#define MAX_REC_VALUE (S16_MAX)

#define REC_ATTR_COUNT (RP_GET_REC_ATTR_COUNT)


// input data file suffixes
#define DATASET_RECORDS_FILEPATH_SUFFIX "_records.csv"
#define DATASET_STATS_FILEPATH_SUFFIX "_stats.csv"

#define DATASET_ATTR_FILEPATH_SUFFIX "_attrs.csv"
#define DATASET_BFS_FILEPATH_SUFFIX "_dag.csv"
#define DATASET_ORDER_FILEPATH_SUFFIX "_order.csv"
#define DATASET_GRPS_FILEPATH_SUFFIX "_grps.csv"

// data structure to represent the metadata
typedef struct _attrcode
{
	u32 idx;
	u16 attr;
	u16 validx;
	string desc;
}
attrcode, *pattrcode;

typedef struct _attrmeta
{
	u16 idx;
	u16 vals;
	string name;
}
attrmeta, *pattrmeta;

typedef struct _budgetmeta
{
	string name;
	double weps;
}
budgetmeta, *pbudgetmeta;

// candidate record info
typedef struct _recfi
{
	u64 seedIdx;
	synthprops props;
}
recfi, *precfi;

// this is the structure for a data record
struct _rawrec
{
	u64 idx;		// for seeds, this is the seedIdx, for fakes it's the fakeIdx
	recfi fi;
	u16* vals;
};

CTVERIFY(sizeof(u16) >= 2, checkRecValsRange)

typedef _rawrec rec, *prec;

#define INVALID_RECORD_VAL (U16_MAX)

// allocate memory and initialize a record
static void allocateRecord(prec r, const u16 v = 0)
{
	VERIFY(v == INVALID_RECORD_VAL || v <= MAX_REC_VALUE);
	VERIFY(REC_ATTR_COUNT > 0 && REC_ATTR_COUNT < MAX_REC_ATTR_COUNT);

	// initialize the record properly
	r->idx = 0;

	// fi
	r->fi.seedIdx = 0;

	// fi.props
	r->fi.props.lnPDF = false;
	r->fi.props.psCount = 0.0;
	r->fi.props.psf = 0.0;
	r->fi.props.ecidx = -1;

	// fi.props.params
	r->fi.props.params = NULL;

	// now create vals
	r->vals = new u16[REC_ATTR_COUNT];
	VERIFY(r->vals != NULL);
	FILL_ARRAY(r->vals, REC_ATTR_COUNT, v);
}

// free memory of record
static void freeRecord(prec r)
{
	VERIFY(r != NULL && r->vals != NULL);
#ifdef SGF_DEBUG
	FILL_ARRAY(r->vals, REC_ATTR_COUNT, INVALID_RECORD_VAL);
#endif
	delete[] r->vals;
	r->vals = NULL;
}

// describe the record as a string.
static string getRecordDesc(const rec* v)
{
	stringstream ss("");

	for(u16 i=0; i<REC_ATTR_COUNT; i++)
	{
		const u16 val = v->vals[i];
		VERIFY(val <= MAX_REC_VALUE);

		if(i > 0) { ss << ", "; }
		ss << (u32)(val + 1);
	}

	return ss.str();
}

class RecordLineWriter : public LineWriter<rec>
{
public:
	RecordLineWriter() { }
	virtual ~RecordLineWriter() {}

	virtual string write(rec v)
	{
		return getRecordDesc(const_cast<const rec*>(&v));
	}
};

class SingleRecordOutputter
{
protected:
	RecordLineWriter* writer;
	string fp;

public:
	SingleRecordOutputter(RecordLineWriter* w, string f, bool trunc = true) : writer(w), fp(f)
	{
		if(trunc == true) { ofstream of(fp.c_str(), ios::trunc); of.close(); }
	}

	virtual ~SingleRecordOutputter() {}

	virtual void output(const rec* r) // output to file
	{
		VERIFY(r != NULL);
		outputRaw(writer->write(*r));
	}

	virtual void outputRaw(const string& str)
	{
		ofstream of(fp.c_str(), ios::app);
		of << str << endl;
		of.close();
	}
};

class RecordOutputter : public Outputter<rec>
{
protected:
	RecordLineWriter* writer;
	string fp;

public:
	RecordOutputter(RecordLineWriter* w, string pref, int pid) : writer(w)
	{
		stringstream ss(""); ss << pref << "/" << pid << ".out";
		fp = ss.str();
	}
	RecordOutputter(RecordLineWriter* w, string f) : writer(w), fp(f)
	{
		;
	}

	virtual ~RecordOutputter() {}

	/** Outputs synthetic candidate alongside with its seed to file. */
	virtual void output(const rec* s, const rec* f)
	{
		VERIFY(f != NULL && s != NULL);

		const synthprops* props = &f->fi.props;
		stringstream ss("");
		ss << f->fi.seedIdx << ", " << f->idx  << ", " << props->params->gam << ", " << props->ecidx << ", " << props->psf << ", " << props->psCount;
		const string rowKey = ss.str();
		{
			ofstream of(fp.c_str(), ios::app);
			of << rowKey << ";" << writer->write(*s) << ";" << writer->write(*f) << endl;
		}
	}
};


// other metadata

typedef struct _bfs
{
	double merit;
	vector<u16> attridx;
}
bfs, *pbfs;

typedef struct _attrgrp
{
	u16 idx;
	map<u16, set<u16>> gmap;
	vector<u16> iv;
}
attrgrp, *pattrgrp;

/** Represents the metadata. */
class Metadata : public Singleton<Metadata>, public LineProcessor<string>, public LineProcessor<double>, public LineProcessor<u16>
{
	friend class Singleton<Metadata>;
private:
	bool verb;
	FileUtils* fu;

	string codesFP;
	string bfsFP;
	string orderFP;
	string grpsFP;

	vector<attrmeta> attrs;
	vector<attrcode> list;
	map<string, attrcode> codes;

	map<string, budgetmeta> budgets;

	vector<bfs> vbfs;
	vector<u16> order;
	vector<attrgrp> vgrps;



	Metadata() : verb(true), fu(FileUtils::getInstance()),
			order(REC_ATTR_COUNT, INVALID_RECORD_VAL) // initialize the vector to already have the correct size
	{
		;
	}
	~Metadata() {}

public:

	/** Initialize the metadata (load it from metadata files.) */
	void initialize(const string dataPath, bool v = true)
	{
		if(v != verb) { verb = v; }
		if(verb) { LOG(INFO) << "Initializing metadata (dataPath: " << dataPath << ")..."; }

		// Codes / Attributes
		codesFP = dataPath + DATASET_ATTR_FILEPATH_SUFFIX;
		if(verb) {  LOG(INFO) << "Reading " << codesFP << "..."; }
		bool success = fu->readLineByLine(codesFP, this);
		if(success == false) { LOG(ERROR) << "Failed to read/parse file " << codesFP; exit(-1); }

		// Best Feature Set / Parents
		bfsFP = dataPath + DATASET_BFS_FILEPATH_SUFFIX;
		if(verb) { LOG(INFO) << "Reading " << bfsFP << "..."; }
		success = fu->readLineByLine<double>(bfsFP, this);
		if(success == false) { LOG(ERROR) << "Failed to read/parse file " << bfsFP; exit(-1); }

		// Order
		orderFP = dataPath + DATASET_ORDER_FILEPATH_SUFFIX;
		if(verb) {  LOG(INFO) << "Reading " << orderFP << "..."; }
		success = fu->readLineByLine<double>(orderFP, this);
		if(success == false) { LOG(ERROR) << "Failed to read/parse file " << orderFP; exit(-1); }

		// check the order vector has all its value correctly initialized
		set<u16> tmpOrder(order.begin(), order.end());
		if(order.size() < REC_ATTR_COUNT || tmpOrder.size() !=  order.size())
		{ LOG(ERROR) << "Error inconsistent order with number of attributes, or invalid order."; exit(-1); }
		for(u16 i=0; i<REC_ATTR_COUNT; i++) { const u16 v = order[i]; VERIFY(v < MAX_REC_VALUE && v != INVALID_RECORD_VAL); }


		// Groups / Bucketization
		grpsFP = dataPath + DATASET_GRPS_FILEPATH_SUFFIX;
		if(verb) { LOG(INFO) << "Reading " << grpsFP << "..."; }
		success = fu->readLineByLine<u16>(grpsFP, this);
		if(success == false) { LOG(ERROR) << "Failed to read/parse file " << grpsFP; exit(-1);  }

		// Privacy Budget
		budgets.clear();
		budgetmeta bm;
		bm.name = "stats";
		bm.weps = Config::getInstance()->get<double>(CONFIG_SYNTH_PRIVACY_GENMODEL_BUDGET);

		budgets[bm.name] = bm;

		VERIFY(vbfs.size() == REC_ATTR_COUNT);
		VERIFY(vgrps.size() == REC_ATTR_COUNT);
		VERIFY(order.size() == REC_ATTR_COUNT);

		if(verb) { LOG(INFO) << "Done with metadata (" << attrs.size() << " attributes, "
			      << list.size() << " codes -- order: " << describeVector<u16>(order) << ")."; }
	}

private:
	void process(const string id, u32 idx, vector<string> values)
	{
		VERIFY(values.size() == 1);

		values = fu->tokenize(values[0], ",");
		string first = values[0];
		values.erase(values.begin());

		if(id.compare(codesFP) == 0)
		{
			if(idx >= REC_ATTR_COUNT) { LOG(ERROR) << "Invalid number of attributes."; exit(-1); }
			VERIFY(idx == attrs.size() && idx < REC_ATTR_COUNT);
			VERIFY(values.size() <= MAX_REC_VALUE);

			attrmeta am;
			am.name = first;
			am.idx = (u16)idx;
			am.vals = (u16)values.size();
			attrs.push_back(am);

			u32 validx=0;
			foreach_const(vector<string>, values, iter)
			{
				string val = *iter;

				attrcode code;
				code.idx = list.size();
				code.attr = idx;
				code.validx = validx;
				code.desc = trim(val);

				list.push_back(code);
				stringstream ss(""); ss << code.attr << ":" << code.validx;
				string key = ss.str();
				codes[key] = code;

				validx++;
			}
		}
		else { CODING_ERROR; }
	}

	void process(const string id, u32 idx, vector<double> values)
	{
		if(id.compare(bfsFP) == 0)
		{
			VERIFY(values.size() >= 1);
			VERIFY(idx < REC_ATTR_COUNT);

			bfs v;
			for(u16 i=0; i<values.size(); i++)
			{
				const double val = values[i];
				if(i < values.size()-1)
				{
					const u16 vaidx = (u16)(val-1);
					VERIFY(vaidx < REC_ATTR_COUNT);

					v.attridx.push_back(vaidx);
				}
				else { v.merit = val; }
			}
			VERIFY(idx == vbfs.size());
			vbfs.push_back(v);

			if(verb)
			{
				LOG(INFO) << "Added bfs for attr " << idx << ", merit: " << v.merit << ", fs: " << describeVector<u16>(v.attridx);
				if(values.size() == 1) { LOG(INFO) << "According to the BFS file: attribute " << idx << " has an empty parent/best features set!"; }
			}
		}
		else if(id.compare(orderFP) == 0)
		{
			VERIFY(values.size() == 1); // order contains only 1 element; the idx of that attr
			VERIFY(idx < REC_ATTR_COUNT);

			const double val = values[0];
			const u16 vaidx = (u16)(val-1);
			VERIFY(vaidx < REC_ATTR_COUNT);

			order[vaidx] = idx;
			if(verb) { LOG(INFO) << "Order for attr " << idx << " -> " << (u32)vaidx; }
		}
		else
		{
			CODING_ERROR; // we should never get here
		}
	}

	void process(const string id, u32 idx, vector<u16> values)
	{
		VERIFY(id.compare(grpsFP) == 0);
		VERIFY(values.size() >= 2 && idx < REC_ATTR_COUNT);

		attrgrp g; g.idx = (u16)idx;
		for(u16 i=0; i<values.size(); i++)
		{
			u16 grpIdx = values[i]; VERIFY(grpIdx >= 1);
			grpIdx--; // subtract one so we get 0-based indices

			map<u16, set<u16>>::iterator iter = MAP_LOOKUP(g.gmap, grpIdx);
			if(iter == g.gmap.end())
			{
				g.gmap[grpIdx] = set<u16>();
				iter = MAP_LOOKUP(g.gmap, grpIdx);
			}
			iter->second.insert(i);

			VERIFY(g.iv.size() == i);
			g.iv.push_back(grpIdx);
		}

		VERIFY(vgrps.size() == idx);
		vgrps.push_back(g);
	}

	pattrcode lookup(string c)
	{
		map<string, attrcode>::iterator iter = MAP_LOOKUP(codes, c);
		if(iter != codes.end()) { return &iter->second; }

		return NULL;
	}

	pattrcode get(u16 idx) { VERIFY(idx < list.size()); return &list[idx]; }

public:
	pbfs getbfs(u16 attridx) { VERIFY(attridx < vbfs.size()); return &vbfs[attridx]; }

	pattrmeta getAttrMetadata(u16 attridx) { VERIFY(attridx < attrs.size()); return &attrs[attridx]; }

	pattrgrp getAttrGrouping(u16 attridx) { VERIFY(attridx < vgrps.size()); return &vgrps[attridx]; }

	pbudgetmeta getBudgetMetadata(const string& name) { VERIFY(MAP_CONTAINS(budgets, name) == true); return &budgets[name]; }

	// this will clear the 'out' vector!
	void getOrder(vector<u16>& out) const { out.clear(); out.insert(out.begin(), order.begin(), order.end()); }

	string getHeaderLine() const
	{
		stringstream ss(""); u32 i=0;
		foreach_const(vector<attrmeta>, attrs, iter)
		{
			if(i > 0) { ss << ", "; }
			ss << iter->name; i++;
		}
		return ss.str();
	}
};

/** Represents a dataset, implemented as a store. */
class Dataset : public Store, public LineProcessor<u16>
{
private:
	FileUtils* fu;
	Metadata* codes;
	string datasetFP; string statsFP;
	string rfp; string rsfp;

	vector<rec> records;
	vector<rec> statsRec;
	vector<prec> recCopy;
	vector<prec> statsCopy;

public:
	Dataset(string f, string k) : Store(k), fu(FileUtils::getInstance()), codes(Metadata::getInstance())
	{
		datasetFP = f + DATASET_RECORDS_FILEPATH_SUFFIX;
		statsFP = f + DATASET_STATS_FILEPATH_SUFFIX;

		stringstream rss(""); rss << k << "/" << "records.dat";
		rfp = rss.str();
		fps.insert(rfp);

		stringstream rsst(""); rsst << k << "/" << "stats.dat";
		rsfp = rsst.str();
		fps.insert(rsfp);
	}
	virtual ~Dataset() {}

	vector<prec>& getRecords()
	{
		if(recCopy.size() != records.size())
		{
			recCopy.clear();
			for(size_t i=0; i<records.size(); i++)
			{
				recCopy.push_back(&records[i]);
			}
		}
		return recCopy;
	}

	vector<prec>& getStats()
	{
		if(statsCopy.size() != statsRec.size())
		{
			statsCopy.clear();
			for(size_t i=0; i<statsRec.size(); i++)
			{
				statsCopy.push_back(&statsRec[i]);
			}
		}
		return statsCopy;
	}

	void process(const string id, u32 idx, vector<u16> values)
	{
		rec r; allocateRecord(&r);
		r.idx = idx; // seedIdx

		VERIFY(values.size() == REC_ATTR_COUNT);
		for(size_t i=0; i<values.size(); i++)
		{
			const u16 v = values[i];
			VERIFY(v >= 1 && v <= MAX_REC_VALUE);

			const u16 validx = v - 1;
			r.vals[i] = validx;
		}

		if(id.compare(datasetFP) == 0)
		{
			VERIFY(idx == records.size());
			records.push_back(r);
		}
		else
		{
			VERIFY(id.compare(statsFP) == 0);
			VERIFY(idx == statsRec.size());
			statsRec.push_back(r);
		}
	}

	bool create()
	{
		// Load the data from records and stats file
		LOG(INFO) << "Reading dataset from " << datasetFP << "...";
		if(fu->readLineByLine<u16>(datasetFP, this, ',', true) == false)
		{ LOG(ERROR) << "Couldn't read/parse file " << datasetFP; return false; }
		if(records.empty()) { LOG(ERROR) << "Parsing file " << datasetFP << " returned an empty dataset."; return false; }

		LOG(INFO) << "Reading stats from " << statsFP << "...";
		if(fu->readLineByLine<u16>(statsFP, this, ',', true) == false) { LOG(ERROR) << "Couldn't read/parse file " << statsFP; return false; }
		if(statsRec.empty()) { LOG(ERROR) << "Parsing file " << statsFP << " returned an empty stats dataset."; return false; }

		LOG(INFO) << "Done.";

		return true;
	}

private:
	bool _store(const string& fp, vector<rec>& recs)
	{
		// store data to disk (compact representation)
		const size_t sz = recs.size();
		vector<u16> recidx; const size_t tsz = sz * REC_ATTR_COUNT;
		u16* rsz = new u16[tsz]; if(rsz == NULL) { LOG(ERROR) << "Failed to allocate memory."; return false; }
		FILL_ARRAY(rsz, tsz, INVALID_RECORD_VAL);

		for(size_t i=0; i<sz; i++)
		{
			prec rec = &recs[i];
			for(u16 j=0; j<REC_ATTR_COUNT; j++)
			{
				size_t idx = GET_INDEX(i, j, REC_ATTR_COUNT);
				VERIFY(idx < tsz);
				rsz[idx] = rec->vals[j];
			}
		}

		// sanity check
		for(size_t i=0; i<tsz; i++) { const u16 v = rsz[i]; VERIFY(v != INVALID_RECORD_VAL && v <= MAX_REC_VALUE); }

		u64 h = hashOf(reinterpret_cast<char*>(rsz), sizeof(u16)*tsz);
		LOG(INFO) << "Storing dataset " << fp << " with checksum: 0x" << hex << setfill('0') << h << dec;

		LOG(INFO) << "Storing pre-processed records to " << fp << "...";
		if(fu->dumpVector(fp, rsz, tsz) == false) { delete[] rsz; return false; }
		delete[] rsz;
		LOG(INFO) << "Done.";

		return true;
	}

public:
	bool store()
	{
		if(_store(rfp, records) == false) { return false; }
		if(_store(rsfp, statsRec) == false) { return false; }

		return true;
	}


private:
	bool _load(const string& fp, vector<rec>& out)
	{
		// load data from disk (compact representation)
		u32 tsz = 0; u16* rsz = NULL;
		LOG(INFO) << "Loading pre-processed dataset from " << fp << "...";
		if(fu->fillVector(fp, &rsz, &tsz) == false) { return false; }

		VERIFY((tsz % REC_ATTR_COUNT) == 0);

		// sanity check
		for(size_t i=0; i<tsz; i++) { const u16 v = rsz[i]; VERIFY(v != INVALID_RECORD_VAL && v <= MAX_REC_VALUE); }

		u64 h = hashOf(reinterpret_cast<char*>(rsz), sizeof(u16)*tsz);
		LOG(INFO) << "Loaded dataset " << fp << " with checksum: 0x" << hex << setfill('0') << h << dec;

		const size_t sz = tsz / REC_ATTR_COUNT;

		for(size_t i=0; i<sz; i++)
		{
			rec rec; allocateRecord(&rec);
			rec.idx = i; // seedIdx
			for(u16 j=0; j<REC_ATTR_COUNT; j++)
			{
				size_t idx = GET_INDEX(i, j, REC_ATTR_COUNT);
				VERIFY(idx < tsz);
				rec.vals[j] = rsz[idx];
			}
			out.push_back(rec);
		}
		delete[] rsz;

		LOG(INFO) << "Done.";

		return true;
	}

public:
	bool load()
	{
		if(_load(rfp, records) == false) { return false; }
		if(_load(rsfp, statsRec) == false) { return false; }
		return true;
	}
};

/** Represents a synthetic dataset. (Used by sgfextract.) */
class SynthDataset : public LineProcessor<string>
{
private:
	FileUtils* fu;
	string dataFP;

	vector<rec> seeds;
	vector<rec> synths;
	vector<u64> pscs;

	bool parseRecord(rec& r, const string& line)
	{
		vector<u16> values;
		bool ok = parseToVector(line, ',', values);
		if(ok == false || values.size() != REC_ATTR_COUNT) { return false; }

		for(size_t i=0; i<values.size(); i++)
		{
			const u16 v = values[i];
			VERIFY(v >= 1 && v <= MAX_REC_VALUE);

			const u16 validx = v - 1;
			r.vals[i] = validx;
		}
		return true;
	}

public:
	SynthDataset(string fp) : fu(FileUtils::getInstance()), dataFP(fp) { ; }

	virtual ~SynthDataset()
	{
		foreach(vector<rec>, seeds, iter) { rec& r = *iter; freeRecord(&r); } seeds.clear();
		foreach(vector<rec>, synths, iter) { rec& r = *iter; freeRecord(&r); } synths.clear();
		pscs.clear();
	}

	bool load()
	{
		LOG(INFO) << "Reading dataset from " << dataFP << "...";
		if(fu->readLineByLine(dataFP, this) == false) { LOG(ERROR) << "Couldn't read/parse file " << dataFP; return false; }
		return true;
	}

	void process(const string id, u32 idx, vector<string> values)
	{
		VERIFY(values.size() == 1 && id.compare(dataFP) == 0);

		string line = values[0];
		const string lineNoSpaces = eraseAllOf(line, ' '); // remove the spaces to facilitate parsing

		vector<string> parts;
		bool ok = parseToVector(lineNoSpaces, ';', parts);
		if(ok == false || parts.size() != 3) { LOG(ERROR) << "Couldn't parse line " << idx << " of file " << id; exit(-1); }

		const string fistr = parts[0];
		const string seedstr = parts[1];
		const string synthstr = parts[2];

		vector<double> dvs;
		ok = parseToVector(fistr, ',', dvs);
		if(ok == false || dvs.size() != 6) { LOG(ERROR) << "Couldn't parse line " << idx << " of file " << id; exit(-1); }

		u64 seedIdx = (u64)dvs[0];
		u64 synthIdx = (u64)dvs[1];

		/*double gam = dvs[2]; // these are not used for now.
		int32 ec = (int32)dvs[3];
		double psf = dvs[4]; */

		u64 psc = (u64)dvs[5];

		rec seed; allocateRecord(&seed);
		seed.idx = seedIdx;

		ok = parseRecord(seed, seedstr);
		if(ok == false) { LOG(ERROR) << "Couldn't parse line " << idx << " of file " << id; exit(-1); }

		rec synth; allocateRecord(&synth);
		synth.idx = synthIdx;

		ok = parseRecord(synth, synthstr);
		if(ok == false) { LOG(ERROR) << "Couldn't parse line " << idx << " of file " << id; exit(-1); }

		VERIFY(idx == synths.size());
		VERIFY(seeds.size() == synths.size() && seeds.size() == pscs.size());

		// push_back
		seeds.push_back(seed);
		synths.push_back(synth);

		pscs.push_back(psc);
	}

	u64 size() { return synths.size(); }

	void get(const size_t idx, rec& seed, rec& synth, u64& psc)
	{
		VERIFY(idx < size());

		seed = seeds[idx];
		synth = synths[idx];
		psc = pscs[idx];
	}
};
