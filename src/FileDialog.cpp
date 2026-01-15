module;

#include <Windows.h>
#include <ShlObj_core.h>
#include <atlbase.h>

module System.FileDialog;

import std;

struct WideFilter
{
	WideFilter() = default;
	WideFilter(const FileDialog::Filter& filter)
	{
		ConvertASCII(filter.description, filter.fileType);
	}

	void ConvertASCII(const std::string& str1, const std::string& str2)
	{
		description = { str1.begin(), str1.end() };
		fileType    = { str2.begin(), str2.end() };
	}

	std::wstring description;
	std::wstring fileType;

	COMDLG_FILTERSPEC GetSystemFilter() // relies on the fact that the instance does not go out of scope before the return value does
	{
		return { description.c_str(), fileType.c_str() };
	}
};

static std::string WideToASCII(const std::wstring& str)
{
	std::string ret;
	ret.reserve(str.size());

	for (wchar_t ch : str)
		ret += static_cast<char>(ch);

	return ret;
}

static std::vector<std::string> RequestOpenDialog(const FileDialog::Filter& filter, const std::string& start, FILEOPENDIALOGOPTIONS foptions)
{
	std::vector<std::string> ret;

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (!SUCCEEDED(hr))
		return ret;

	WideFilter wFilter(filter);
	COMDLG_FILTERSPEC winfilter = wFilter.GetSystemFilter();
	CComPtr<IFileOpenDialog> dialog;
	
	hr = dialog.CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER);
	if (!SUCCEEDED(hr))
		return ret;

	dialog->SetFileTypes(1, &winfilter);

	std::wstring wdir = std::wstring(start.begin(), start.end());

	IShellItem* pFolder = nullptr;
	if (start.empty())
	{
		hr = SHCreateItemFromParsingName(wdir.c_str(), NULL, IID_PPV_ARGS(&pFolder));
		if (!SUCCEEDED(hr))
		{
			CoUninitialize();
			return ret;
		}
	}

	DWORD options = 0;
	dialog->GetOptions(&options);
	dialog->SetOptions(options | foptions);

	dialog->SetFolder(pFolder);
	if (pFolder)
		pFolder->Release();

	dialog->Show(NULL);

	CComPtr<IShellItemArray> items;
	hr = dialog->GetResults(&items.p);
	if (!SUCCEEDED(hr))
		return ret;

	DWORD count = 0;
	items->GetCount(&count);

	if (count == 0)
		return ret;
	ret.reserve(count);

	for (DWORD i = 0; i < count; i++)
	{
		wchar_t* wpath = nullptr;
		IShellItem* item = nullptr; // i dont know if i have to release these items too ??

		items->GetItemAt(i, &item);
		item->GetDisplayName(SIGDN_FILESYSPATH, &wpath);
		if (wpath == nullptr)
			continue;

		std::string path = WideToASCII(wpath);
		ret.emplace_back(path);
	}
	CoUninitialize();
	return ret;
}

static std::string RequestSaveDialog(const FileDialog::Filter& filter, const std::string& start, FILEOPENDIALOGOPTIONS foptions, FILEOPENDIALOGOPTIONS removeOptions)
{
	std::string ret;

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (!SUCCEEDED(hr))
		return ret;

	WideFilter wFilter(filter);
	COMDLG_FILTERSPEC winfilter = wFilter.GetSystemFilter();
	CComPtr<IFileSaveDialog> dialog;

	hr = dialog.CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER);
	if (!SUCCEEDED(hr))
		return ret;

	dialog->SetFileTypes(1, &winfilter);

	std::wstring wdir = std::wstring(start.begin(), start.end());

	IShellItem* pFolder = nullptr;
	if (start.empty())
	{
		hr = SHCreateItemFromParsingName(wdir.c_str(), NULL, IID_PPV_ARGS(&pFolder));
		if (!SUCCEEDED(hr))
		{
			CoUninitialize();
			return ret;
		}
	}

	DWORD options = 0;
	dialog->GetOptions(&options);
	options = options & ~(removeOptions);

	dialog->SetOptions(options | foptions);

	dialog->SetFolder(pFolder);
	if (pFolder)
		pFolder->Release();

	dialog->Show(NULL);

	IShellItem* item = nullptr; // i dont know if i have to release these items too ??
	hr = dialog->GetResult(&item);
	if (!SUCCEEDED(hr) || item == nullptr)
		return ret;

	wchar_t* wpath = nullptr;
	item->GetDisplayName(SIGDN_FILESYSPATH, &wpath);
	if (wpath == nullptr)
		return ret;

	ret = WideToASCII(wpath);

	CoTaskMemFree(wpath);
	CoUninitialize();
	return ret;
}

std::vector<std::string> FileDialog::RequestFiles(const Filter& filter, const std::string& start)
{
	std::vector<std::string> ret = RequestOpenDialog(filter, start, FOS_ALLOWMULTISELECT | FOS_STRICTFILETYPES);
	if (ret.empty())
		ret.push_back("");
	return ret;
}

std::vector<std::string> FileDialog::RequestFolders(const Filter& filter, const std::string& start)
{
	std::vector<std::string> ret = RequestOpenDialog(filter, start, FOS_ALLOWMULTISELECT | FOS_PICKFOLDERS);
	if (ret.empty())
		ret.push_back("");
	return ret;
}

std::string FileDialog::RequestFile(const Filter& filter, const std::string& start)
{
	std::vector<std::string> ret = RequestOpenDialog(filter, start, FOS_STRICTFILETYPES);
	return ret.empty() ? "" : ret[0];
}

std::string FileDialog::RequestFolder(const Filter& filter, const std::string& start)
{
	std::vector<std::string> ret = RequestOpenDialog(filter, start, FOS_PICKFOLDERS);
	return ret.empty() ? "" : ret[0];
}

std::string FileDialog::RequestFileSaveLocation(const Filter& filter, const std::string& start)
{
	std::string ret = RequestSaveDialog(filter, start, FOS_STRICTFILETYPES, FOS_OVERWRITEPROMPT);
	return ret.empty() ? "" : ret;
}