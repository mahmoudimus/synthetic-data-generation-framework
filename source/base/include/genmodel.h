/** Abstract definition of a generative model.
 * The two key methods are propose() and pdf(). They must be consistent with each other for the model to be meaningful. **/
#ifndef SGFBASE_GENMODEL_H_
#define SGFBASE_GENMODEL_H_

/** Abstract class for a generative model. */
template<class R> class GenerativeModel
{
public:
	GenerativeModel() {}
	virtual ~GenerativeModel() {}

	/** Returns a new synthetic candidate based on the model and the given seed. */
	virtual R* propose(const R* seed) = 0;

	/** Returns the probability (or natural log of if lnpdf() == true) that the synthetic candidate is produced through propose() given the seed and the model. */
	virtual double pdf(const R* seed, const R* fake) = 0;

	/** True if pdf() returns log(prob), false otherwise. */
	virtual bool lnpdf() = 0;

	virtual void initialize() { ; }
	virtual void shutdown() { ; }

	/** true if both propose() and pdf() do *not* depend on the given seed, false otherwise. */
	virtual bool isSeedless() = 0;
};

#endif /* SGFBASE_GENMODEL_H_ */
