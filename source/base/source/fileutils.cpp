#include "defs.h"

int64 FileUtils::getFileSize(const string fp)
{
    ifstream in(fp.c_str(), ifstream::ate | ifstream::binary);
    return in.tellg();
}

bool FileUtils::readLineByLine(const string fp, LineProcessor<string>* proc)
{
	ifstream input(fp.c_str());
	if(input.good() == false) { LOG(WARNING) << "Failed to open file " << fp; return false; }
	u32 i=0;
	for(string line; getline(input, line);)
	{
		vector<string> values;
		values.push_back(line);
		proc->process(fp, i, values);
		i++;
	}
	return true;
}

bool FileUtils::extractGroups(const string line, const char start, const char end, vector<string>& groups)
{
	string tmp = line;
	size_t spos = tmp.find(start); size_t epos = tmp.find(end);
	while(spos < epos && epos != string::npos)
	{
		size_t len = epos - (spos + 1);
		const string group = tmp.substr(spos+1, len);

		groups.push_back(group);

		tmp = tmp.substr(epos + 1);

		spos = tmp.find(start);
		epos = tmp.find(end);
	}
	return true;
}

vector<string> FileUtils::tokenize(const string line, string delim)
{
	vector<string> ret;

	string tmp = line;
	size_t pos = tmp.find(delim);

	while(pos != string::npos)
	{
		const string token = tmp.substr(0, pos);
		ret.push_back(token);
		tmp = tmp.substr(pos+delim.length());

		pos = tmp.find(delim);
	}
	ret.push_back(tmp); // the final one;

	return ret;
}
