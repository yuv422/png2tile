#include <vector>
#include <map>
#include <cstdint>
#include <cstring>

// Forward declares
void deinterleave(std::vector<uint8_t>& buf, uint32_t interleaving);
void compressTile(const std::vector<uint8_t>& src, std::vector<uint8_t>& dest);

const char* PSGaiden_getName()
{
	// A pretty name for this compression type
	// Generally, the name of the game it was REd from
	return "PS Gaiden";
}

const char* PSGaiden_getExt()
{
	// A string suitable for use as a file extension
	return "psgcompr";
}

int PSGaiden_compressTiles(uint8_t* source, uint32_t numTiles, uint8_t* dest, uint32_t destLength)
{
	if (numTiles > 0xffff)
	{
		return -1; // error
	}
	std::vector<uint8_t> buf; // the output
	buf.reserve(destLength); // avoid reallocation

	// Write number of tiles
	buf.push_back((numTiles >> 0) & 0xff);
	buf.push_back((numTiles >> 8) & 0xff);

	std::vector<uint8_t> tile;
	tile.resize(32); // zero fill
	for (uint32_t i = 0; i < numTiles; ++i)
	{
		// Get tile into buffer
		std::copy(source, source + 32, tile.begin());
		source += 32;
		// Deinterleave it
		deinterleave(tile, 4);
		// Compress it to dest
		compressTile(tile, buf);
	}

	// check length
	if (buf.size() > destLength)
	{
		return 0;
	}
	// copy to dest
	memcpy(dest, &buf[0], buf.size());
	// return length
	return buf.size();
}

void deinterleave(std::vector<uint8_t>& buf, uint32_t interleaving)
{
	std::vector<uint8_t> tempbuf(buf.size());

	// Deinterleave into tempbuf
	int bitplanesize = buf.size() / interleaving;
	for (unsigned int src = 0; src < buf.size(); ++src)
	{
		// If interleaving is 4 I want to turn
		// AbcdEfghIjklMnopQrstUvwx
		// into
		// AEIMQUbfjnrvcgkoswdhlptx
		// so for a char at position x
		// x div 4 = offset within this section
		// x mod 4 = which section
		// final position = (x div 4) + (x mod 4) * (section size)
		unsigned int dest = src / interleaving + (src % interleaving) * bitplanesize;
		tempbuf[dest] = buf[src];
	}

	// Copy results over the original data
	std::copy(tempbuf.begin(), tempbuf.end(), buf.begin());
}

void findMostCommonValue(std::vector<uint8_t>::const_iterator data, uint8_t& value, int& count)
{
	std::map<uint8_t, int> counts;
	// count occurences of each value
	for (int i = 0; i < 8; ++i)
	{
		uint8_t val = *data++;
		counts[val]++;
	}
	// find the highest count and its value
	count = 0;
	for (auto& pair: counts)
	{
		if (pair.second > count)
		{
			value = pair.first;
			count = pair.second;
		}
	}
}

int countMatches(std::vector<uint8_t>::const_iterator me, std::vector<uint8_t>::const_iterator other, bool invert)
{
	int count = 0;
	int mask = invert ? 0xff : 0x00;
	for (int i = 0; i < 8; ++i)
	{
		if (*me++ == (*other++ ^ mask))
		{
			++count;
		}
	}
	return count;
}

void compressTile(const std::vector<uint8_t>& src, std::vector<uint8_t>& dest)
{
	uint8_t bitplaneMethods = 0;

	// compress the tile data into a temporary buffer
	// because we need to build the method byte before outputting to dest
	std::vector<uint8_t> tiledata;

	// for each bitplane
	for (int bitplaneIndex = 0; bitplaneIndex < 4; ++bitplaneIndex)
	{
		// Find the most common value in this bitplane
		uint8_t mostCommonByte;
		int mostCommonByteCount;
		std::vector<uint8_t>::const_iterator itBitplane = src.begin() + bitplaneIndex * 8;
		findMostCommonValue(itBitplane, mostCommonByte, mostCommonByteCount);

		// Find how much it matches previous bitplanes
		uint8_t otherBitplaneMatchIndex = 0; // which bitplane
		int otherBitplaneMatchCount = 0; // how many matched
		bool otherBitplaneMatchInverse = false; // whether it was an inverted match
		for (uint8_t otherBitplaneIndex = 0; otherBitplaneIndex < bitplaneIndex; ++otherBitplaneIndex)
		{
			// Compare
			std::vector<uint8_t>::const_iterator itOtherBitplane = src.begin() + otherBitplaneIndex * 8;
			int count = countMatches(itBitplane, itOtherBitplane, false);
			if (count > otherBitplaneMatchCount)
			{
				otherBitplaneMatchIndex = otherBitplaneIndex;
				otherBitplaneMatchCount = count;
				otherBitplaneMatchInverse = false;
			}
			count = countMatches(itBitplane, itOtherBitplane, true);
			if (count > otherBitplaneMatchCount)
			{
				otherBitplaneMatchIndex = otherBitplaneIndex;
				otherBitplaneMatchCount = count;
				otherBitplaneMatchInverse = true;
			}
		}

		// Choose method and shift into bitplaneMethods
		bitplaneMethods <<= 2;

		if (mostCommonByteCount == 8 && mostCommonByte == 0x00)
		{
			// all 0 = %00
			bitplaneMethods |= 0x00;
			// no data
		}
		else if (mostCommonByteCount == 8 && mostCommonByte == 0xff)
		{
			// all ff = %01
			bitplaneMethods |= 0x01;
			// no data
		}
		else if (mostCommonByteCount <= 2 && otherBitplaneMatchCount <= 2)
		{
			// raw = %11
			bitplaneMethods |= 0x03;
			// output raw data
			tiledata.insert(tiledata.end(), itBitplane, itBitplane + 8);
		}
		else
		{
			// compressed = %10
			bitplaneMethods |= 0x02;

			// select compression type
			if (otherBitplaneMatchCount == 8)
			{
				/// %000f00nn = whole bitplane duplicate
				if (otherBitplaneMatchInverse)
				{
					tiledata.push_back(0x10 | otherBitplaneMatchIndex);
				}
				else
				{
					tiledata.push_back(0x00 | otherBitplaneMatchIndex);
				}
			}
			else if (otherBitplaneMatchCount > mostCommonByteCount)
			{
				/// %001000nn = copy bytes
				/// %010000nn = copy and invert bytes
				if (otherBitplaneMatchInverse)
				{
					tiledata.push_back(0x40 | otherBitplaneMatchIndex);
				}
				else
				{
					tiledata.push_back(0x20 | otherBitplaneMatchIndex);
				}

				// Build bitmask
				uint8_t bitmask = 0;
				uint8_t mask = otherBitplaneMatchInverse ? 0xff : 0x00;
				std::vector<uint8_t>::const_iterator it1 = itBitplane;
				std::vector<uint8_t>::const_iterator it2 = src.begin() + otherBitplaneMatchIndex * 8;
				for (int i = 0; i < 8; ++i , ++it1 , ++it2)
				{
					bitmask <<= 1;
					if (*it1 == (*it2 ^ mask))
					{
						bitmask |= 1;
					}
				}

				// output byte
				tiledata.push_back(bitmask);

				// output non-matching bytes
				it1 = itBitplane;
				it2 = src.begin() + otherBitplaneMatchIndex * 8;
				for (int i = 0; i < 8; ++i , ++it1 , ++it2)
				{
					bitmask <<= 1;
					if (*it1 != (*it2 ^ mask))
					{
						tiledata.push_back(*it1);
					}
				}
			}
			else
			{
				// common byte
				// build bitmask
				uint8_t bitmask = 0;
				std::vector<uint8_t>::const_iterator it = itBitplane;
				for (int i = 0; i < 8; ++i , ++it)
				{
					bitmask <<= 1;
					if (*it == mostCommonByte)
					{
						bitmask |= 1;
					}
				}
				tiledata.push_back(bitmask);
				tiledata.push_back(mostCommonByte);

				// output non-matching bytes
				it = itBitplane;
				for (int i = 0; i < 8; ++i , ++it)
				{
					bitmask <<= 1;
					if (*it != mostCommonByte)
					{
						tiledata.push_back(*it);
					}
				}
			} // compression method selection
		} // method selection
	} // bitplane loop

	// output
	dest.push_back(bitplaneMethods);
	dest.insert(dest.end(), tiledata.begin(), tiledata.end());
}

