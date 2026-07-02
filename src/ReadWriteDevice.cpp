module IO.ReadWriteDevice;

import std;

ReadWriteDevice::ReadWriteDevice(const std::string_view& file, ReadWriteFile::OpenMethod method)
{
	if (file.empty())
		variant = BinaryStream();
	else
		variant = ReadWriteFile(file, method);
}

bool ReadWriteDevice::Read(char* dst, unsigned long count) const
{
	return std::visit([&](auto& f) 
		{ 
			if constexpr (std::is_same_v<ReadWriteFile, std::decay_t<decltype(f)>>)
				return f.Read(dst, count);

			return false;
		}, variant);
}

bool ReadWriteDevice::Write(const char* src, unsigned long count)
{
	return std::visit([&](auto& f) { return f.Write(src, count); }, variant);
}

bool ReadWriteDevice::IsInFileMode() const
{
	return std::holds_alternative<ReadWriteFile>(variant);
}

bool ReadWriteDevice::IsInMemoryMode() const
{
	return std::holds_alternative<BinaryStream>(variant);
}

std::size_t ReadWriteDevice::GetSize() const
{
	return std::visit([&](auto& f)
		{
			if constexpr (std::is_same_v<ReadWriteFile, std::decay_t<decltype(f)>>)
				return f.GetFileSize();
			else
				return f.data.size();
		}, variant);
}

std::int64_t ReadWriteDevice::SeekG(std::int64_t index, ReadWriteFile::Method method) const
{
	return std::visit([&](auto& f)
		{
			if constexpr (std::is_same_v<ReadWriteFile, std::decay_t<decltype(f)>>)
				return f.SeekG(index, method);
			else
				return static_cast<std::int64_t>(f.GetOffset());
		}, variant);
}

bool ReadWriteDevice::IsValid() const
{
	return std::visit([&](auto& f)
		{
			if constexpr (std::is_same_v<ReadWriteFile, std::decay_t<decltype(f)>>)
				return f.IsValid();
			else
				return true;
		}, variant);
}

std::optional<WriteSession> ReadWriteDevice::CreateWriteSession()
{
	return std::visit([&](auto& f)
		{
			std::optional<WriteSession> ret;
			if constexpr (std::is_same_v<ReadWriteFile, std::decay_t<decltype(f)>>)
				ret.emplace(f);

			return ret;
		}, variant);
}

std::optional<ReadSession> ReadWriteDevice::CreateReadSession()
{
	return std::visit([&](auto& f)
		{
			std::optional<ReadSession> ret;
			if constexpr (std::is_same_v<ReadWriteFile, std::decay_t<decltype(f)>>)
				ret.emplace(f);

			return ret;
		}, variant);
}