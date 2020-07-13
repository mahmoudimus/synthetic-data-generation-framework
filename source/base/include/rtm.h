/** Provides runtime measurements functionality (e.g., timing measurements). **/
#ifndef SGFBASE_RTM_H_
#define SGFBASE_RTM_H_

typedef struct _rtm
{
	string name;
	vector<double> values;
}
rtm, *prtm;

class RTM : public Singleton<RTM>
{
	friend class Singleton<RTM>;

private:
	map<string, rtm> measurements;

	RTM() { ; }

	virtual ~RTM() {}

public:

	/** Adds new sample for measurement by name. */
	void add(const string& name, const double v)
	{
		map<string, rtm>::iterator iter = MAP_LOOKUP(measurements, name);
		if(iter == measurements.end())
		{
			rtm rtm; rtm.name = name; rtm.values.clear();
			measurements[name] = rtm;
			iter = MAP_LOOKUP(measurements, name);
		}
		iter->second.values.push_back(v);
	}

	/** Write out all measurements to log. */
	void dumpToLog()
	{
		MathUtils* mu = MathUtils::getInstance();
		pair_foreach_const(map<string, rtm>, measurements, iter)
		{
			const string& name = iter->first;
			const rtm& rtm = iter->second;

			descstats s;
			s.max = 0.0; s.mean = 0.0; s.min = 0.0; s.n = 0.0; s.std = 0.0; s.sum = 0.0;
			mu->summarize(rtm.values, &s);

			LOG(INFO) << "RTM: [" << name << "]  n: " << (u64)s.n << fixed << setprecision(6)
					<< ", mean +- std: " << s.mean << " +- " << s.std
					<< " (min: " << s.min << ", max: " << s.max << ", sum: " << s.sum << ")";
		}
	}

};


#endif // SGFBASE_RTM_H_
