#include "stdafx.h"
#include "JsonParser.h"

int JsonDoc::Alloc()
{
	int idx = (int)m_pool.size();
	m_pool.push_back(JsonVal());
	return idx;
}

JsonVal *JsonDoc::Root()
{
	return m_root >= 0 ? &m_pool[m_root] : NULL;
}

JsonVal *JsonDoc::Node(int idx)
{
	return idx >= 0 && idx < (int)m_pool.size() ? &m_pool[idx] : NULL;
}

void JsonDoc::SkipWS()
{
	while (*m_p == _T(' ') || *m_p == _T('\t') || *m_p == _T('\r') || *m_p == _T('\n'))
		m_p++;
}

std::wstring JsonDoc::ParseStringToken()
{
	m_p++;
	std::wstring result;
	while (*m_p && *m_p != _T('"'))
	{
		if (*m_p == _T('\\'))
		{
			m_p++;
			switch (*m_p)
			{
			case _T('"'): result += _T('"'); break;
			case _T('\\'): result += _T('\\'); break;
			case _T('/'): result += _T('/'); break;
			case _T('b'): result += _T('\b'); break;
			case _T('f'): result += _T('\f'); break;
			case _T('n'): result += _T('\n'); break;
			case _T('r'): result += _T('\r'); break;
			case _T('t'): result += _T('\t'); break;
			case _T('u'):
				{
					m_p++;
					unsigned int cp = 0;
					for (int i = 0; i < 4 && *m_p; i++, m_p++)
					{
						cp <<= 4;
						if (*m_p >= _T('0') && *m_p <= _T('9')) cp += *m_p - _T('0');
						else if (*m_p >= _T('a') && *m_p <= _T('f')) cp += *m_p - _T('a') + 10;
						else if (*m_p >= _T('A') && *m_p <= _T('F')) cp += *m_p - _T('A') + 10;
					}
					m_p--;
					if (cp >= 0xD800 && cp <= 0xDBFF)
					{
						if (m_p[1] == _T('\\') && m_p[2] == _T('u'))
						{
							m_p += 3;
							unsigned int cp2 = 0;
							for (int i = 0; i < 4 && *m_p; i++, m_p++)
							{
								cp2 <<= 4;
								if (*m_p >= _T('0') && *m_p <= _T('9')) cp2 += *m_p - _T('0');
								else if (*m_p >= _T('a') && *m_p <= _T('f')) cp2 += *m_p - _T('a') + 10;
								else if (*m_p >= _T('A') && *m_p <= _T('F')) cp2 += *m_p - _T('A') + 10;
							}
							m_p--;
							cp = 0x10000 + ((cp - 0xD800) << 10) + (cp2 - 0xDC00);
						}
					}
					if (cp <= 0xFFFF)
						result += (wchar_t)cp;
					else
					{
						cp -= 0x10000;
						result += (wchar_t)(0xD800 + (cp >> 10));
						result += (wchar_t)(0xDC00 + (cp & 0x3FF));
					}
				}
				break;
			default: result += *m_p; break;
			}
			m_p++;
		}
		else
		{
			result += *m_p;
			m_p++;
		}
	}
	if (*m_p == _T('"'))
		m_p++;
	return result;
}

double JsonDoc::ParseNumberToken()
{
	const TCHAR *start = m_p;
	if (*m_p == _T('-')) m_p++;
	while (*m_p >= _T('0') && *m_p <= _T('9')) m_p++;
	if (*m_p == _T('.')) { m_p++; while (*m_p >= _T('0') && *m_p <= _T('9')) m_p++; }
	if (*m_p == _T('e') || *m_p == _T('E')) { m_p++; if (*m_p == _T('+') || *m_p == _T('-')) m_p++; while (*m_p >= _T('0') && *m_p <= _T('9')) m_p++; }
	std::wstring numStr(start, m_p - start);
	return _wtof(numStr.c_str());
}

int JsonDoc::ParseArray()
{
	int idx = Alloc();
	m_pool[idx].type = JsonVal::Array;
	m_p++;
	SkipWS();
	if (*m_p == _T(']')) { m_p++; return idx; }
	while (true)
	{
		int child = ParseValue();
		if (child < 0) return -1;
		m_pool[idx].children.push_back(child);
		SkipWS();
		if (*m_p == _T(',')) { m_p++; SkipWS(); continue; }
		if (*m_p == _T(']')) { m_p++; return idx; }
		return -1;
	}
}

int JsonDoc::ParseObject()
{
	int idx = Alloc();
	m_pool[idx].type = JsonVal::Object;
	m_p++;
	SkipWS();
	if (*m_p == _T('}')) { m_p++; return idx; }
	while (true)
	{
		SkipWS();
		if (*m_p != _T('"')) return -1;
		std::wstring key = ParseStringToken();
		SkipWS();
		if (*m_p != _T(':')) return -1;
		m_p++;
		SkipWS();
		int val = ParseValue();
		if (val < 0) return -1;
		m_pool[idx].keys.push_back(key);
		m_pool[idx].children.push_back(val);
		SkipWS();
		if (*m_p == _T(',')) { m_p++; SkipWS(); continue; }
		if (*m_p == _T('}')) { m_p++; return idx; }
		return -1;
	}
}

int JsonDoc::ParseValue()
{
	SkipWS();
	if (!*m_p) return -1;
	switch (*m_p)
	{
	case _T('"'):
		{
			int idx = Alloc();
			m_pool[idx].type = JsonVal::String;
			m_pool[idx].str = ParseStringToken();
			return idx;
		}
	case _T('{'): return ParseObject();
	case _T('['): return ParseArray();
	case _T('t'):
		{
			int idx = Alloc();
			m_pool[idx].type = JsonVal::Bool;
			m_pool[idx].boolean = true;
			m_p += 4;
			return idx;
		}
	case _T('f'):
		{
			int idx = Alloc();
			m_pool[idx].type = JsonVal::Bool;
			m_pool[idx].boolean = false;
			m_p += 5;
			return idx;
		}
	case _T('n'):
		{
			int idx = Alloc();
			m_pool[idx].type = JsonVal::Null;
			m_p += 4;
			return idx;
		}
	default:
		{
			int idx = Alloc();
			m_pool[idx].type = JsonVal::Number;
			m_pool[idx].num = ParseNumberToken();
			return idx;
		}
	}
}

bool JsonDoc::Parse(const TCHAR *p)
{
	m_pool.clear();
	m_root = -1;
	if (!p) return false;
	m_p = p;
	m_root = ParseValue();
	return m_root >= 0;
}

int JsonDoc::FindKey(int objIdx, const std::wstring &key)
{
	if (objIdx < 0 || objIdx >= (int)m_pool.size()) return -1;
	JsonVal &v = m_pool[objIdx];
	if (v.type != JsonVal::Object) return -1;
	for (int i = 0; i < (int)v.keys.size(); i++)
		if (v.keys[i] == key) return v.children[i];
	return -1;
}

std::wstring JsonDoc::GetStr(const std::wstring &key, const std::wstring &defVal)
{
	int idx = FindKey(m_root, key);
	if (idx < 0 || m_pool[idx].type != JsonVal::String) return defVal;
	return m_pool[idx].str;
}

double JsonDoc::GetDbl(const std::wstring &key, double defVal)
{
	int idx = FindKey(m_root, key);
	if (idx < 0 || m_pool[idx].type != JsonVal::Number) return defVal;
	return m_pool[idx].num;
}

int JsonDoc::GetInt(const std::wstring &key, int defVal)
{
	return (int)GetDbl(key, defVal);
}

int JsonDoc::GetArrLen(const std::wstring &key)
{
	int idx = FindKey(m_root, key);
	if (idx < 0 || m_pool[idx].type != JsonVal::Array) return 0;
	return (int)m_pool[idx].children.size();
}

int JsonDoc::GetArrAt(const std::wstring &key, int i)
{
	int idx = FindKey(m_root, key);
	if (idx < 0 || m_pool[idx].type != JsonVal::Array) return -1;
	if (i < 0 || i >= (int)m_pool[idx].children.size()) return -1;
	return m_pool[idx].children[i];
}

std::wstring JsonDoc::NGetStr(int idx, const std::wstring &key, const std::wstring &defVal)
{
	int v = FindKey(idx, key);
	if (v < 0 || m_pool[v].type != JsonVal::String) return defVal;
	return m_pool[v].str;
}

double JsonDoc::NGetDbl(int idx, const std::wstring &key, double defVal)
{
	int v = FindKey(idx, key);
	if (v < 0 || m_pool[v].type != JsonVal::Number) return defVal;
	return m_pool[v].num;
}

int JsonDoc::NGetInt(int idx, const std::wstring &key, int defVal)
{
	return (int)NGetDbl(idx, key, defVal);
}

int JsonDoc::NGetArrLen(int idx, const std::wstring &key)
{
	int v = FindKey(idx, key);
	if (v < 0 || m_pool[v].type != JsonVal::Array) return 0;
	return (int)m_pool[v].children.size();
}

int JsonDoc::NGetArrAt(int idx, const std::wstring &key, int i)
{
	int v = FindKey(idx, key);
	if (v < 0 || m_pool[v].type != JsonVal::Array) return -1;
	if (i < 0 || i >= (int)m_pool[v].children.size()) return -1;
	return m_pool[v].children[i];
}

int JsonDoc::NCount(int idx)
{
	if (idx < 0 || idx >= (int)m_pool.size()) return 0;
	if (m_pool[idx].type != JsonVal::Array) return 0;
	return (int)m_pool[idx].children.size();
}

int JsonDoc::NAt(int idx, int i)
{
	if (idx < 0 || idx >= (int)m_pool.size()) return -1;
	if (m_pool[idx].type != JsonVal::Array) return -1;
	if (i < 0 || i >= (int)m_pool[idx].children.size()) return -1;
	return m_pool[idx].children[i];
}
