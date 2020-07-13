/** Provides utility functions for math stuff. **/
#ifndef SGFBASE_MATHUTILS_H_
#define SGFBASE_MATHUTILS_H_

/** Summary statistics / description of a sample. */
typedef struct _descstats
{
	double n;
	double min;
	double max;
	double mean;
	double std;
	double sum;
}
descstats, *pdescstats;
class MathUtils : public Singleton<MathUtils>
{
	friend class Singleton<MathUtils>;

private:
	MathUtils() { rng = RNG::getInstance(); }

	virtual ~MathUtils() {}
	RNG* rng;

public:

	void setRNGSeed(u64 newSeed) { rng->seedPRNG(newSeed); }

	void resetRNGSeed()	{ rng->defSeedPRNG(); }

	/** Returns a uniform random double in (0,1). */
	double uniformSample() { return rng->uniformRandomDouble(); }

	/** Returns a random sample from Laplace with shape parameter b > 0 and mu = 0.0. */
	double laplaceSample(const double b) { return laplaceSample(0, b); }
	/** Returns a random sample from Laplace with shape parameter b > 0 and mu. */
	double laplaceSample(const double mu, const double b)
	{
		const double u = uniformSample() - 0.5;
		const double su = SIGN(u);
		const double il = 1.0 - 2.0 * abs(u);
		const double lu = log(il);
		return mu - b * su * lu;
	}

	/** Returns a random sample according to the geometric mechanism (see the Ghosh et al. paper). */
	double geomDPSample(const double alpha)
	{
		// Pr[|ret| = t] = [(1 - alpha) / (1 + alpha)] * alpha^t
		double absret = 0;

		const double calpha = (1.0 - alpha) / (1.0 + alpha);

		const double u = uniformSample();
		double prev = 0.0; double sum = 0.0;
		while(sum < u)
		{
			double inc = pow(alpha, absret);
			if(absret > 0) { inc *= 2.0; } // when absret > 0, then this is the probability of outputting both -ret and +ret.

			sum = calpha * (prev + inc);
			prev += inc;

			if(sum < u) { absret++; }
		}
		const double u2 = uniformSample();
		return ((absret == 0) || (u2 >= 0.5)) ? +absret : -absret;
	}

	/** Returns a random sample from N(0, sigma). */
	double gaussianSample(const double sigma)
	{
		return gaussianSample(0, sigma);
	}

	/** Returns a random sample from N(mu, sigma). */
	double gaussianSample(const double mu, const double sigma)
	{
		return mu + sigma * standardNormalSample();
	}

	/** Returns a random sample from N(0, 1) -- standard normal. */
	double standardNormalSample()
	{
		double x = 0.0;
		double y = 0.0;

		double norm = 0.0;
		do
		{
			/* choose x,y in uniform square (-1,-1) to (+1,+1) */
			x = -1.0 + 2.0 * uniformSample();
			y = -1.0 + 2.0 * uniformSample();

			/* see if it is in the unit circle */
			norm = x * x + y * y;
		}
		while(norm > 1.0 || norm <= 0);

		/* Box-Muller transform */
		return y * sqrt(-2.0 * log(norm) / norm);
	}

	/** Normalize the given vector. (Sum of the entries will add up to 1.0.) */
	void normalize(double* v, u32 sz)
	{
		double sum = 0.0;
		for(u64 i=0; i<sz; i++) { sum += v[i]; }

		if(sum > 0) { for(u64 i=0; i<sz; i++) { v[i] /= sum; } }
	}

	/** Samples a random index from vector given their associated probability. Note: assumes p is a vector of probabilities summing to 1.0. */
	u32 sampleFromVector(const double* p, u32 sz)
	{
		VERIFY(p != NULL && sz > 0);
		const double u = uniformSample();
		double sum = 0.0;
		for(u32 i=0; i<sz; i++)
		{
			sum += p[i];
			if(u < sum) { return i; }
		}
		VERIFY((sum + EPSILON) >= 1.0);
		return sz-1; // return the last index, so we never sample an invalid value
	}

	/** Returns a uniformly random integer between min and max (inclusive). */
	u32 uniformInteger(u32 min, u32 max)
	{
		return rng->uniformRandomULLBetween((u64)min, (u64)max);
	}

	/** See: https://en.wikipedia.org/wiki/Beta_function */
	double beta(double x, double y)
	{
		return tgamma(x) * tgamma(y) / tgamma(x + y);
	}
	double lbeta(double x, double y)
	{
		return lgamma(x) + lgamma(y) - lgamma(x + y);
	}

	/** Returns the probabability of the specified multinomial outcome given the Dirichlet distribution represented by alpha. */
	double dirichletMultinomialSingleTrialPMF(const double* alpha, const u32 K, const u32 idx)
	{
		double* theta = new double[K];
		dirichletExpectedValue(alpha, K, theta);
		const double ret = theta[idx];

		delete[] theta;

		return ret;
	}

	void dirichletRandomSampleSmall(const double* alpha, const u64 K, double* theta)
	{
		/*  When the values of alpha[] are small, scale the variates to avoid
			underflow so that the result is not 0/0.  Note that the Dirichlet
			distribution is defined by a ratio of gamma functions so we can
			take out an arbitrary factor to keep the values in the range of
			double precision. */

		VERIFY(alpha != NULL && K != 0 && theta != NULL);

		double umax = 0;
		for(u64 i = 0; i < K; i++)
		{
			double u = log(uniformSample()) / alpha[i];
			theta[i] = u;

			if (u > umax || i == 0) { umax = u; }
		}

		for(u64 i = 0; i < K; i++)
		{
			double e = theta[i] - umax;
			double r = exp(e);
			bool err = (errno == ERANGE) || (errno == -ERANGE);
			if(err == true) { r = SQRT_DBL_MIN; }

			theta[i] = r;
		}

		for(u64 i = 0; i < K; i++)
		{
			// FIX 02.04.2012: if alpha[i] is 0.0, then theta[i] should be 0.0
			if(alpha[i] == 0.0) { theta[i] = 0.0; }
			else { theta[i] = theta[i] * rng->gammaRandomDouble(alpha[i] + 1.0, 1.0); }
		}

		double norm = 0.0;
		for(u64 i = 0; i < K; i++) { norm += theta[i]; }
		for(u64 i = 0; i < K; i++) { theta[i] /= norm; }
	}

	void dirichletRandomSample(const double* alpha, const u32 K, double* theta)
	{
		VERIFY(alpha != NULL && K != 0 && theta != NULL);

		ZERO_VECTOR(theta, K);

		double norm = 0.0;
		bool underflow = false;
		for(u64 i = 0; i < K; i++)
		{
			double g = rng->gammaRandomDouble(alpha[i], 1.0);
			if((g == 0) && (alpha[i] > 0)) { underflow = true; } // underflow

			theta[i] = g;
		}

		for(u64 i = 0; i < K; i++)
		{
			norm += theta[i];
		}

		if(norm < SQRT_DBL_MIN || underflow == true)  /* Handle underflow */
		{
			dirichletRandomSampleSmall(alpha, K, theta);
			return;
		}

		bool renormalize = false;
		for(u64 i = 0; i < K; i++)
		{
			double t = theta[i];
			theta[i] /= norm;
			if(t > 0 && theta[i] == 0) { theta[i] = SQRT_DBL_MIN; renormalize = true;}  // prevent underflow
		}

		if(renormalize == true)
		{
			norm = 0.0;
			for(u64 i = 0; i < K; i++)	{ norm += theta[i]; }
			for(u64 i = 0; i < K; i++) { theta[i] /= norm; }
		}
	}

	/** Computes the expected value of Dirichlet given alpha. */
	void dirichletExpectedValue(const double* alpha, const u32 K, double* theta)
	{
		VERIFY(alpha != NULL && K != 0 && theta != NULL);

		ZERO_VECTOR(theta, K);

		double sum = 0.0;
		for(u64 i=0; i < K; i++) { sum += alpha[i]; }
		for(u64 i=0; i < K; i++) { theta[i] = alpha[i] / sum; }
	}

	/** Sample a multinomial outcome from the distribution according to the given Dirichlet vector alpha. */
	u32 sampleDirichletMultinomialFromVector(const double* alpha, const u32 K)
	{
		double* theta = new double[K];
		dirichletRandomSample(alpha, K, theta);

		const u32 ret = sampleFromVector(theta, K);

		delete[] theta;

		return ret;
	}

	void summarize(vector<double> vals, pdescstats stats)
	{
		VERIFY(stats != NULL);

		stats->n = (double)vals.size();
		stats->min = DBL_MAX;
		stats->max = DBL_MIN;

		double sum = 0.0;
		foreach_const(vector<double>, vals, iter)
		{
			const double v = *iter;
			sum += v;
			if(v < stats->min) { stats->min = v; }
			if(v > stats->max) { stats->max = v; }
		}
		stats->sum = sum;
		stats->mean = (sum / stats->n);

		double std = 0.0;
		foreach_const(vector<double>, vals, iter)
		{
			const double v = *iter;

			std += pow(v - stats->mean, 2.0);
		}
		std /= stats->n;
		stats->std = sqrt(std);
	}
};

#endif /* SGFBASE_MATHUTILS_H_ */
