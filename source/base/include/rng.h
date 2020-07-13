/** Provides RNG functionality.
 * Uses ISAAC (risaac.h) to ensure cryptographic strength. (See: http://burtleburtle.net/bob/rand/isaac.html) **/
#ifndef SGFBASE_RNG_H_
#define SGFBASE_RNG_H_

#define RND_MAX_IDX (RANDSIZ)

typedef struct _SEEDED_PRNG_CONTEXT
{
	randctx ctx;
	size_t nextIdx;
}
PRNG_CONTEXT, *PPRNG_CONTEXT;

class RNG : public Singleton<RNG>
{
  friend class Singleton<RNG>;
  private:
    RNG();

    PRNG_CONTEXT prng;

    inline uint64 randomUINT64();

  public:
    void seedPRNG(uint64 seed);
    void defSeedPRNG();

    double uniformRandomDouble();
    ull uniformRandomULLBetween(ull min, ull max);

    ull uniformRandomULL();

    void randomString(uchar* out, ull len);

    template<typename T>
    bool randomPermutation(T* inout, const ull count)
    {
    	if(inout == NULL || count == 0) { return false; }
		if(count == 1) { return true; }

		for(ull i = 0; i < count - 1; i++)
		{
			size_t idx = uniformRandomULLBetween(i, count - 1);

			// swap
			T tmp = inout[idx];
			inout[idx] = inout[i];
			inout[i] = tmp;
		}
		return true;
    }

    double gaussianRandomDouble(double sigma);
    double gammaRandomDouble(double a, double b);
};

#endif /* SGFBASE_RNG_H_ */
