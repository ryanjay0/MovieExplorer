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

	std::wstring S(const std::wstring &key, const std::wstring &def = L"");
	double D(const std::wstring &key, double def = 0);
	int I(const std::wstring &key, int def = 0);
	int ArrLen(const std::wstring &key);
	int ArrAt(const std::wstring &key, int i);

	std::wstring NS(int idx, const std::wstring &key, const std::wstring &def = L"");
	double ND(int idx, const std::wstring &key, double def = 0);
	int NI(int idx, const std::wstring &key, int def = 0);
	int NArrLen(int idx, const std::wstring &key);
	int NArrAt(int idx, const std::wstring &key, int i);
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
	int FindKey(int objIdx, const std::wstring &key);
};
