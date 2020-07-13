/** Provides utility functions for manipulate files. **/
#ifndef SGFBASE_FILEUTILS_H_
#define SGFBASE_FILEUTILS_H_

template <typename T>
class LineProcessor
{
public:
	LineProcessor() {}
	virtual ~LineProcessor() {}

	virtual void process(const string id, u32 idx, vector<T> values) = 0;
};

template <typename T>
class LineWriter
{
public:
	LineWriter() {}
	virtual ~LineWriter() {}

	virtual string write(T v) = 0;
};

class FileUtils : public Singleton<FileUtils>
{
	friend class Singleton<FileUtils>;

private:
	FileUtils() {}
	virtual ~FileUtils() {}

public:
	/** Read and parse text file line by line with the given LineProcessor */
	template <typename T>
	bool readLineByLine(const string fp, LineProcessor<T>* proc, const char delim=',', bool skipHeader = false);

	/** Read and parse text file line by line using the whole line as a string */
	bool readLineByLine(const string fp, LineProcessor<string>* proc);

	/** Write dataset to text file line by line using the given LineWriter for formatting. */
	template <typename T>
	bool writeLineByLine(const string fp, LineWriter<T>* writer, const vector<T>& dataset);

	/** Get the file size (<0 on failure.) */
	int64 getFileSize(const string fp);

	bool fileExists(const string fp) { return (getFileSize(fp) >= 0); }

	/** Write vector data to compact file representation. Note: file format is specific to architecture (endianness). */
	template <typename T>
	bool dumpVector(const string fp, T* vec, u32 sz);

	/** Read vector data from compact file representation. Note: file format is specific to architecture (endianness). */
	template <typename T>
	bool fillVector(const string fp, T** vec, u32* sz);

	/** Extract substrings given the specifed group start and end delimiters. */
	bool extractGroups(const string line, const char start, const char end, vector<string>& groups);

	/** Tokenize a string given the specified delimiter. */
	vector<string> tokenize(const string line, string delim);
};

template <typename T>
bool FileUtils::readLineByLine(const string fp, LineProcessor<T>* proc, const char delim, bool skipHeader)
{
	ifstream input(fp.c_str());
	if(input.good() == false) { LOG(WARNING) << "Failed to open file " << fp; return false; }

	u32 i=0; vector<T> values;
	for(string line; getline(input, line);)
	{
		if(i == 0 && skipHeader == true) { i++; continue; }

		values.clear();
		bool ok = parseToVector(line, delim, values);
		if(ok == false) { return false; }

		int64 effIdx = (skipHeader == true) ? i-1 : i; VERIFY(effIdx >= 0);
		proc->process(fp, (u32)effIdx, values);
		i++;
	}
	return true;
}

template <typename T>
bool FileUtils::writeLineByLine(const string fp, LineWriter<T>* writer, const vector<T>& dataset)
{
	ofstream output(fp.c_str());

	for(u32 i=0; i<dataset.size(); i++)
	{
		T rec = dataset[i];
		string str = writer->write(rec);
		str += "\n";
		output.write(str.c_str(), str.length());
	}

	return true;
}

template <typename T>
bool FileUtils::dumpVector(const string fp, T* vec, u32 sz)
{
	VERIFY(vec != NULL);
	ofstream output(fp.c_str(), ios::out | ios::binary);
	if(output.fail() == true) { return false; }

	u32 nsz = sz;
	output.write(reinterpret_cast<char*>(&nsz), sizeof(u32));  if(output.fail() == true) { return false; }
	for(u32 i=0; i<sz; i++)
	{
		T v = vec[i];
		output.write(reinterpret_cast<char*>(&v), sizeof(T));
		if(output.fail() == true) { return false; }
	}

	return true;
}

template <typename T>
bool FileUtils::fillVector(const string fp, T** vec, u32* sz)
{
	VERIFY(vec != NULL);
	ifstream input(fp.c_str(), ios::in | ios::binary);

	if(input.fail() == true) { return false; }
	input.read((char*)sz, sizeof(u32));
	if(input.fail() == true) { return false; }

	const u32 nsz = *sz;
	T* pv = (*vec) = new T[nsz];
	if(pv == NULL) { LOG(ERROR) << "Failed to allocate memory."; return false; }
	for(u32 i=0; i<nsz; i++)
	{
		T v;
		input.read(reinterpret_cast<char*>(&v), sizeof(T));
		if(input.fail() == true) { return false; }
		pv[i] = v;
	}

	return true;
}

#endif /* SGFBASE_FILEUTILS_H_ */
