/** Provides runtime parameters functionality. **/
#ifndef SGFBASE_PARAMS_H_
#define SGFBASE_PARAMS_H_

// Note: this epsilon has nothing to do with differential privacy; it's simply a small value used for floating point comparisons
#define EPSILON (0.000001)

#define RP_GET_REC_ATTR_COUNT (RunParameters::getInstance()->getRecAttrCount())

class RunParameters: public Singleton<RunParameters>
{
	friend class Singleton<RunParameters>;

private:
	u16 debugLevel;
	u16 recAttrCount;

	RunParameters() : debugLevel(DEBUG_NONE_LEVEL), recAttrCount(0)
	{
		;
	}
	~RunParameters() {}

public:
	const static u16 DEBUG_NONE_LEVEL = 0x0;
	const static u16 DEBUG_INFO_LEVEL = 0x10;
	const static u16 DEBUG_VERBOSE_LEVEL = 0x20;


	void setDebugLevel(u16 lvl) { debugLevel = lvl; }

	bool equalDebugLevel(u16 lvl) const { return debugLevel >= lvl; }


	// should be called only once at initialization!!!
	void setRecAttrCount(u16 c) { VERIFY(c > 0 && recAttrCount == 0); recAttrCount = c; }
	u16 getRecAttrCount() { return recAttrCount; }
};

#endif /* SGFBASE_PARAMS_H_ */
