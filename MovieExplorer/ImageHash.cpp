#include "stdafx.h"
#include "ImageHash.h"
#include "MovieExplorer.h"

ImageHash::ImageHash()
{ 
}

ImageHash::~ImageHash()
{
}

ARBYTE* ImageHash::GetImage(RString strName)
{
	std::wstring sz = std::wstring((LPCWSTR)strName);
	RLock lock(&cs_);
	std::unordered_map<std::wstring, ARBYTE>::iterator got = hashtable.find(sz);

	if (got == hashtable.end())
		return NULL;

	return &got->second;
}

void ImageHash::SetImage(RString strName, ARBYTE image)
{
	std::wstring sz = std::wstring((LPCWSTR)strName);
	RLock lock(&cs_);
	hashtable[sz] = image;
}
