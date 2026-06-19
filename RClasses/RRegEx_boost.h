#ifndef __RREGEX_H__
#define __RREGEX_H__

#define REGEX_MULTILINE				1
#define REGEX_CASEINSENSITIVE		2
#define REGEX_PRESEARCH				4

#include <regex>

class RRegEx
{
public:
	RRegEx()
		{}

	RRegEx(const TCHAR *lpszPatternStart, const TCHAR *lpszPatternEnd, 
		   UINT options = REGEX_MULTILINE|REGEX_CASEINSENSITIVE|REGEX_PRESEARCH)
		{Create(lpszPatternStart, lpszPatternEnd, options);}

	RRegEx(const TCHAR *lpszPattern, INT_PTR options = REGEX_MULTILINE|REGEX_CASEINSENSITIVE|REGEX_PRESEARCH)
		{Create(lpszPattern, options);}

	RRegEx(RString_ strPattern, INT_PTR options = REGEX_MULTILINE|REGEX_CASEINSENSITIVE|REGEX_PRESEARCH)
		{Create(strPattern, strPattern + strPattern.GetLength(), options);}

	~RRegEx()
		{}

	bool Create(const TCHAR *lpszPatternStart, const TCHAR *lpszPatternEnd, 
				INT_PTR options = REGEX_MULTILINE|REGEX_CASEINSENSITIVE|REGEX_PRESEARCH)
	{
		Destroy();

		try
		{
			auto flags = std::regex::ECMAScript;
			if (options & REGEX_CASEINSENSITIVE)
				flags |= std::regex::icase;

			m_regex = std::basic_regex<TCHAR>(lpszPatternStart, lpszPatternEnd, flags);
			return true;
		}
		catch (const std::regex_error&)
		{
			return false;
		}
	}

	bool Create(const TCHAR *lpszPattern, INT_PTR options = REGEX_MULTILINE|REGEX_CASEINSENSITIVE|REGEX_PRESEARCH)
		{return Create(lpszPattern, lpszPattern + _tcslen(lpszPattern), options);}

	bool Create(RString_ strPattern, INT_PTR options = REGEX_MULTILINE|REGEX_CASEINSENSITIVE|REGEX_PRESEARCH)
		{return Create(strPattern, strPattern + strPattern.GetLength(), options);}

	void Destroy()
	{
		m_regex = std::basic_regex<TCHAR>();
		m_results = std::match_results<const TCHAR*>();
	}

	bool Search(const TCHAR *lpszTextStart, const TCHAR *lpszTextEnd, const TCHAR **ppszMatchStart = NULL, 
				const TCHAR **ppszMatchEnd = NULL)
	{
		if (m_regex.mark_count() == 0)
			{ASSERT(false); return false;}

		m_results = std::match_results<const TCHAR*>();

		try
		{
			auto flags = std::regex_constants::match_default;
			if (!std::regex_search(lpszTextStart, lpszTextEnd, m_results, m_regex, flags))
				return false;
		}
		catch (const std::regex_error&)
		{
			return false;
		}

		if (ppszMatchStart)
			*ppszMatchStart = m_results[0].first;
		if (ppszMatchEnd)
			*ppszMatchEnd = m_results[0].second;
		return true;
	}

	bool Search(const TCHAR *lpszText, const TCHAR **ppszMatchStart = NULL, const TCHAR **ppszMatchEnd = NULL)
		{return Search(lpszText, lpszText + _tcslen(lpszText), ppszMatchStart, ppszMatchEnd);}

	bool Search(RString_ strText, RString *pstrMatch = NULL)
	{
		if (Search(strText, (const TCHAR*)strText + strText.GetLength()))
		{
			if (pstrMatch)
			{
				size_t cch = m_results[0].second - m_results[0].first;
				memcpy(pstrMatch->GetBuffer(cch), m_results[0].first, cch * sizeof(TCHAR));
				pstrMatch->ReleaseBuffer(cch);
			}
			return true;
		}
		return false;
	}

	bool GetMatch(INT_PTR nIndex, const TCHAR **ppszStart, const TCHAR **ppszEnd) const
	{
		if (nIndex >= (INT_PTR)m_results.size())
			{ASSERT(false); return false;}

		if (ppszStart)
			*ppszStart = m_results[(int)nIndex].first;
		if (ppszEnd)
			*ppszEnd = m_results[(int)nIndex].second;

		return true;
	}

	bool GetMatch(INT_PTR nIndex, RString *pstrMatch) const
	{
		if (nIndex >= (INT_PTR)m_results.size())
			{ASSERT(false); return false;}
		
		if (pstrMatch)
		{
			size_t cch = m_results[(int)nIndex].second - m_results[(int)nIndex].first;
			if (cch == 0)
				pstrMatch->Empty();
			else
			{
				memcpy(pstrMatch->GetBuffer(cch), m_results[(int)nIndex].first, cch * sizeof(TCHAR));
				pstrMatch->ReleaseBuffer(cch);
			}
		}

		return true;
	}

	RString GetMatch(INT_PTR nIndex) const
	{
		RString str;
		GetMatch(nIndex, &str);
		return str;
	}

	INT_PTR GetMatchCount() const
	{
		return (INT_PTR)m_results.size();
	}

	RString Replace(RString_ strText, RString_ strReplaceBy)
	{
		std::basic_string<TCHAR> result = std::regex_replace((std::basic_string<TCHAR>)strText, m_regex, (std::basic_string<TCHAR>)strReplaceBy);
		return result.c_str();
	}

protected:
	std::basic_regex<TCHAR> m_regex;
	std::match_results<const TCHAR*> m_results;
};

#endif // __RREGEX_H__
