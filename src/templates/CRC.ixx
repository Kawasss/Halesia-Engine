export module Templates.CRC;

import std;

import <boost/crc.hpp>;

template<typename T>
concept Fundamental = std::is_fundamental_v<T>;

using CrcUint64 = boost::crc_optimal<64, 0x42F0E1EBA9EA369, std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max(), true, true>;

export template<Fundamental T>
std::uint64_t GetChecksum(const std::span<const T>& data)
{
	CrcUint64 crc;
	crc.process_bytes(data.data(), data.size() * sizeof(T));
	return crc.checksum();
}