/** Provides an abstract definition of a class to output synthetic candidates. **/
#ifndef SGFBASE_OUTPUTTER_H_
#define SGFBASE_OUTPUTTER_H_

template <class T>
class Outputter
{
public:
	Outputter() {}
	virtual ~Outputter() {}

	virtual void output(const T* s, const T* f) = 0;
};

#endif /* SGFBASE_OUTPUTTER_H_ */
