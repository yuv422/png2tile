#include <vector>
#include <map>
#include <cstdint>
#include <cstring>
#include <algorithm>

void deinterleave(std::vector<uint8_t>& buffer, uint32_t interleaving)
{
    std::vector<uint8_t> tempBuffer(buffer.size());

    // Deinterleave into tempBuffer
    const auto bitplaneSize = buffer.size() / interleaving;
    for (unsigned int src = 0; src < buffer.size(); ++src)
    {
        // ReSharper disable CommentTypo
        // If interleaving is 4 I want to turn
        // AbcdEfghIjklMnopQrstUvwx
        // into
        // AEIMQUbfjnrvcgkoswdhlptx
        // so for a char at position x
        // x div 4 = offset within this section
        // x mod 4 = which section
        // final position = (x div 4) + (x mod 4) * (section size)
        // ReSharper restore CommentTypo
        const size_t dest = src / interleaving + (src % interleaving) * bitplaneSize;
        tempBuffer[dest] = buffer[src];
    }

    // Copy results over the original data
    std::copy(tempBuffer.begin(), tempBuffer.end(), buffer.begin());
}

void findMostCommonValue(std::vector<uint8_t>::const_iterator data, uint8_t& value, int& count)
{
    std::map<uint8_t, int> counts;
    // count occurrences of each value
    for (int i = 0; i < 8; ++i)
    {
        uint8_t val = *data++;
        counts[val]++;
    }
    // find the highest count and its value
    count = 0;
    for (const auto& pair : counts)
    {
        if (pair.second > count)
        {
            value = pair.first;
            count = pair.second;
        }
    }
}

int countMatches(std::vector<uint8_t>::const_iterator me, std::vector<uint8_t>::const_iterator other, const bool invert)
{
    int count = 0;
    const int mask = invert ? 0xff : 0x00;
    for (int i = 0; i < 8; ++i)
    {
        if (*me++ == (*other++ ^ mask))
        {
            ++count;
        }
    }
    return count;
}

void compressTile(const std::vector<uint8_t>& source, std::vector<uint8_t>& destination)
{
    uint8_t bitplaneMethods = 0;

    // compress the tile data into a temporary buffer
    // because we need to build the method byte before outputting to destination
    std::vector<uint8_t> tileData;

    // for each bitplane
    for (int bitplaneIndex = 0; bitplaneIndex < 4; ++bitplaneIndex)
    {
        // Find the most common value in this bitplane
        uint8_t mostCommonByte;
        int mostCommonByteCount;
        auto itBitplane = source.begin() + bitplaneIndex * 8;
        findMostCommonValue(itBitplane, mostCommonByte, mostCommonByteCount);

        // Find how much it matches previous bitplanes
        int otherBitplaneMatchIndex = 0; // which bitplane
        int otherBitplaneMatchCount = 0; // how many matched
        bool otherBitplaneMatchInverse = false; // whether it was an inverted match
        for (int otherBitplaneIndex = 0; otherBitplaneIndex < bitplaneIndex; ++otherBitplaneIndex)
        {
            // Compare
            const auto itOtherBitplane = source.begin() + otherBitplaneIndex * 8;
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
            tileData.insert(tileData.end(), itBitplane, itBitplane + 8);
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
                    tileData.push_back(static_cast<uint8_t>(0x10 | otherBitplaneMatchIndex));
                }
                else
                {
                    tileData.push_back(static_cast<uint8_t>(0x00 | otherBitplaneMatchIndex));
                }
            }
            else if (otherBitplaneMatchCount > mostCommonByteCount)
            {
                /// %001000nn = copy bytes
                /// %010000nn = copy and invert bytes
                if (otherBitplaneMatchInverse)
                {
                    tileData.push_back(static_cast<uint8_t>(0x40 | otherBitplaneMatchIndex));
                }
                else
                {
                    tileData.push_back(static_cast<const unsigned char&>(0x20 | otherBitplaneMatchIndex));
                }

                // Build bitmask
                uint8_t bitmask = 0;
                const uint8_t mask = otherBitplaneMatchInverse ? 0xff : 0x00;
                auto it1 = itBitplane;
                auto it2 = source.begin() + otherBitplaneMatchIndex * 8;
                for (int i = 0; i < 8; ++i, ++it1, ++it2)
                {
                    bitmask <<= 1;
                    if (*it1 == (*it2 ^ mask))
                    {
                        bitmask |= 1;
                    }
                }

                // output byte
                tileData.push_back(bitmask);

                // output non-matching bytes
                it1 = itBitplane;
                it2 = source.begin() + otherBitplaneMatchIndex * 8;
                for (int i = 0; i < 8; ++i, ++it1, ++it2)
                {
                    bitmask <<= 1;
                    if (*it1 != (*it2 ^ mask))
                    {
                        tileData.push_back(*it1);
                    }
                }
            }
            else
            {
                // common byte
                // build bitmask
                uint8_t bitmask = 0;
                auto it = itBitplane;
                for (int i = 0; i < 8; ++i, ++it)
                {
                    bitmask <<= 1;
                    if (*it == mostCommonByte)
                    {
                        bitmask |= 1;
                    }
                }
                tileData.push_back(bitmask);
                tileData.push_back(mostCommonByte);

                // output non-matching bytes
                it = itBitplane;
                for (int i = 0; i < 8; ++i, ++it)
                {
                    bitmask <<= 1;
                    if (*it != mostCommonByte)
                    {
                        tileData.push_back(*it);
                    }
                }
            } // compression method selection
        } // method selection
    } // bitplane loop

    // output
    destination.push_back(bitplaneMethods);
    destination.insert(destination.end(), tileData.begin(), tileData.end());
}

const char* PSGaiden_getName()
{
    // A pretty name for this compression type
    // Generally, the name of the game it was REd from
    // ReSharper disable once StringLiteralTypo
    return "PS Gaiden";
}

const char* PSGaiden_getExt()
{
    // A string suitable for use as a file extension
    // ReSharper disable once StringLiteralTypo
    return "psgcompr";
}

int PSGaiden_compressTiles(
    const uint8_t* pSource,
    const uint32_t numTiles,
    uint8_t* pDestination,
    const uint32_t destinationLength)
{
    if (numTiles > 0xffff)
    {
        return -1; // error
    }
    std::vector<uint8_t> destination; // the output
    destination.reserve(destinationLength); // avoid reallocation

    // Write number of tiles
    destination.push_back((numTiles >> 0) & 0xff);
    destination.push_back((numTiles >> 8) & 0xff);

    std::vector<uint8_t> tile;
    tile.resize(32); // zero fill
    for (uint32_t i = 0; i < numTiles; ++i)
    {
        // Get tile into buffer
        std::copy_n(pSource, 32, tile.begin());
        pSource += 32;
        // Deinterleave it
        deinterleave(tile, 4);
        // Compress it to dest
        compressTile(tile, destination);
    }

    // check length
    if (destination.size() > destinationLength)
    {
        return 0;
    }
    // copy to dest
    memcpy(pDestination, &destination[0], destination.size());
    // return length
    return static_cast<int>(destination.size());
}
