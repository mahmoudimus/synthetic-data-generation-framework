/** Abstract definition of a persistent file storage class. **/
#ifndef SGFBASE_STORE_H_
#define SGFBASE_STORE_H_

class Store
{
protected:
	FileUtils* fu;
	string key;
	set<string> fps;

public:
	Store(string k)
	{
		fu = FileUtils::getInstance();
		key = k;
	}
	virtual ~Store() { }

	/** Check if the store has already data written to disk. */
	bool onDisk();

	bool initialize();

	/** Create the store. */
	virtual bool create() = 0;

	/** Load data from disk. */
	virtual bool load() = 0;

	/** Write data to disk. */
	virtual bool store() = 0;
};

#endif /* SGFBASE_STORE_H_ */
