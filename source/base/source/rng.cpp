#include "defs.h"

RNG::RNG()
{
	defSeedPRNG();
}

void RNG::seedPRNG(uint64 seed)
{
	ZeroMemory(&prng.ctx, sizeof(randctx));
	prng.ctx.randrsl[0]=(ub4)(seed >> 32); prng.ctx.randrsl[1]=(ub4)(seed & 0xFFFFFFFF);
	randinit(&prng.ctx, 1);
	prng.nextIdx = 0;
}

void RNG::defSeedPRNG()
{
    unsigned long seed = mix(clock(), time(NULL), getPID());
	seedPRNG(seed);
}

uint64 RNG::randomUINT64()
{
	PPRNG_CONTEXT p = &prng;
	uint64 ret = ( (((uint64)p->ctx.randrsl[p->nextIdx]) << 32) | ((uint64)p->ctx.randrsl[p->nextIdx+1]) ); p->nextIdx+=2;
	if(p->nextIdx >= RND_MAX_IDX-1) { isaac(&p->ctx); p->nextIdx = 0; }

	return ret;
}

double RNG::uniformRandomDouble()
{
	double r = 0.0;
	double mult = (double)(1.0 / (double)(ULLONG_MAX+0.0));

	do
	{
		r = (double)randomUINT64() * mult;
	}
	while(r <= 0.0 || r >= 1.0); // rejection sampling to make the bounds exclusive: i.e. r \in ]0, 1[

	return r;
}

ull RNG::uniformRandomULL()
{
	return (ull)randomUINT64();
}

ull RNG::uniformRandomULLBetween(ull min, ull max)
{
	return (ull)(min + floor((double)((max - min + 1) * uniformRandomDouble())));
}


void RNG::randomString(uchar* out, ull len)
{
	ull r = 0; char* p = (char*)out; char* chr = NULL;
	for(ull i = 0; i < len; i++)
	{
		ushort rem = (i % sizeof(ull));
		if(rem == 0) { r = uniformRandomULL(); chr = (char*)(&r); }

		*p = (char)(chr[rem]); p++;
	}
}

double RNG::gaussianRandomDouble(double sigma)
{
	double x = 0.0;
	double y = 0.0;

	double norm = 0.0;
	do
	{
		/* choose x,y in uniform square (-1,-1) to (+1,+1) */
		x = -1.0 + 2.0 * uniformRandomDouble();
		y = -1.0 + 2.0 * uniformRandomDouble();

		/* see if it is in the unit circle */
		norm = x * x + y * y;
	}
	while(norm > 1.0 || norm <= 0);

	/* Box-Muller transform */
	return sigma * y * sqrt(-2.0 * log(norm) / norm);
}

double RNG::gammaRandomDouble(double a, double b)
{
	if(a == 0.0) { return 0.0; }

	double u = 0.0;
	if(a < 1.0)
	{
		u = uniformRandomDouble();
		double e = 1.0 / a;
		double p = pow(u, e);
		// bool err = (errno == ERANGE) || (errno == -ERANGE);
		if(p < SQRT_DBL_MIN) { p = SQRT_DBL_MIN; }

		double rec = gammaRandomDouble(1.0 + a, b);
		return (double)(rec * p);
	}

	double x = 0.0;
	double v = 0.0;
	double d = a - 1.0 / 3.0;
	double c = (1.0 / 3.0) / sqrt(d);

	while(true)
	{
		do
		{
			x = gaussianRandomDouble(1.0);
			v = 1.0 + c * x;
		}
		while (v <= 0.0);

		v = v * v * v;
		u = uniformRandomDouble();

		if(u < (1.0 - 0.0331 * x * x * x * x)) { break; }

		if(log(u) < (0.5 * x * x + d * (1.0 - v + log(v)))) { break; }
	}

	return b * d * v;
}

