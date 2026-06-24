#pragma once

#include <vector>
#include <string>

struct JsonVal
{
	enum Type { Null, Bool, Number, String, Array, Object };
	Type type;
	std::wstring str;
	double num;
	bool boolean;
	std::vector<std::wstring> keys;
	std::vector<int> children;

	JsonVal() : type(Null), num(0), boolean(false) {}
};

class JsonDoc
{
public:
	bool Parse(const TCHAR *p);
	JsonVal *Root();
	JsonVal *Node(int idx);

	int RootIdx() const { return m_root; }

	int FindKey(int objIdx, const std::wstring &key);

	std::wstring GetStr(const std::wstring &key, const std::wstring &defVal = L"");
	double GetDbl(const std::wstring &key, double defVal = 0);
	int GetInt(const std::wstring &key, int defVal = 0);
	int GetArrLen(const std::wstring &key);
	int GetArrAt(const std::wstring &key, int i);

	std::wstring NGetStr(int idx, const std::wstring &key, const std::wstring &defVal = L"");
	double NGetDbl(int idx, const std::wstring &key, double defVal = 0);
	int NGetInt(int idx, const std::wstring &key, int defVal = 0);
	int NGetArrLen(int idx, const std::wstring &key);
	int NGetArrAt(int idx, const std::wstring &key, int i);
	int NCount(int idx);
	int NAt(int idx, int i);

private:
	std::vector<JsonVal> m_pool;
	int m_root;
	const TCHAR *m_p;

	int Alloc();
	int ParseValue();
	int ParseArray();
	int ParseObject();
	std::wstring ParseStringToken();
	double ParseNumberToken();
	void SkipWS();
};
