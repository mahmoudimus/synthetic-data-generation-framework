/** Provides miscellaneous utility functions. **/
#ifndef SGFBASE_MISC_H_
#define SGFBASE_MISC_H_

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

// trim from start
static inline std::string& ltrim(std::string &s) { s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace)))); return s; }

// trim from end
static inline std::string& rtrim(std::string &s) { s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end()); return s; }

// trim from both ends
static inline std::string& trim(std::string &s) { return ltrim(rtrim(s)); }

/** Removes all occurrences of character in string. */
static inline string eraseAllOf(string& s, char c)
{
	string ret = s;
	size_t pos = 0;
	do
	{
		pos = ret.find(c);
		if(pos != string::npos) { ret.erase(pos, 1); }
	}
	while(pos != string::npos);
	return ret;
}

template <typename T>
static inline string describeSet(set<T>& s)
{
	stringstream ss(""); ss << "{";
	typename set<T>::iterator iter = s.begin();
	for(u64 i=0; i<s.size(); i++)
	{
		if(i > 0) { ss << ","; }
		ss << *iter;

		advance(iter, 1);
	}
	ss << "}";
	return ss.str();
}

template <typename T>
static inline string describeVector(vector<T>& s, const string& delim = ",", const bool raw = false)
{
	stringstream ss("");
	if(raw == false) { ss << "("; }

	typename vector<T>::iterator iter = s.begin();
	for(u64 i=0; i<s.size(); i++)
	{
		if(i > 0) { ss << delim; }
		ss << *iter;

		advance(iter, 1);
	}
	if(raw == false) { ss << ")"; }
	return ss.str();
}

template <typename T>
static inline string describeArray(T* d, u32 sz)
{
	stringstream ss(""); ss << "[";
	for(u64 i=0; i<sz; i++)
	{
		if(i > 0) { ss << ","; }
		ss << d[i];
	}
	ss << "]";
	return ss.str();
}

template <typename T>
static inline bool parseToVector(const string str, const char delim, vector<T>& values)
{
	size_t currentPos = 0;
	do
	{
		T value;
		size_t pos = str.find(delim, currentPos);

		string sstr = str.substr(currentPos, (pos == string::npos ? pos : (pos - currentPos)));
		stringstream ss(trim(sstr));
		ss >> value;

		if(ss.fail() == true) { return false; }

		values.push_back(value);

		if(pos == string::npos) { break; }

		currentPos = pos + 1; // field delimiter is 1 char
	}
	while(true);

	return true;
}

template <typename T>
static inline T parseString(const string& v)
{
	T ret;
	stringstream ss(""); ss << v; ss >> ret;
	return ret;
}

static inline int getPID()
{
	return (int)getpid();
}

/** See: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function */
static inline u64 hashOf(const char* c, const size_t len)
{
	/**
	 * hash = FNV_offset_basis
	   for each byte_of_data to be hashed
			hash = hash XOR byte_of_data
			hash = hash × FNV_prime
	   return hash
	 *  */
	const u64 FNV_offset_basis = 0xcbf29ce484222325;
	const u64 FNV_prime = 0x100000001b3;

	u64 ret = FNV_offset_basis;
	for(size_t i=0; i<len; i++)
	{
		ret = ret ^ ((u64)c[i]);
		ret = ret * FNV_prime;
	}

	// do it again in reverse
	for(int64 i=len-1; i>=0; i--)
	{
		ret = ret ^ ((u64)c[i]);
		ret = ret * FNV_prime;
	}

	return ret;
}

static inline u64 hashOf(const string& str)
{
	return hashOf(str.c_str(), str.length());
}

// http://www.concentric.net/~Ttwang/tech/inthash.htm
static inline unsigned long mix(unsigned long a, unsigned long b, unsigned long c)
{
    a=a-b;  a=a-c;  a=a^(c >> 13);
    b=b-c;  b=b-a;  b=b^(a << 8);
    c=c-a;  c=c-b;  c=c^(b >> 13);
    a=a-b;  a=a-c;  a=a^(c >> 12);
    b=b-c;  b=b-a;  b=b^(a << 16);
    c=c-a;  c=c-b;  c=c^(b >> 5);
    a=a-b;  a=a-c;  a=a^(c >> 3);
    b=b-c;  b=b-a;  b=b^(a << 10);
    c=c-a;  c=c-b;  c=c^(b >> 15);
    return c;
}

#endif /* SGFBASE_MISC_H_ */
