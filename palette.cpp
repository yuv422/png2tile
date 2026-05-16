/*
The MIT License (MIT)

Copyright (c) 2016-2026 Eric Fry

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "palette.h"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

std::set<int> setUnion(const std::set<int>& a, const std::set<int>& b) {
    std::set<int> result = a;
    result.insert(b.begin(), b.end());
    return result;
}

bool backtrack(
    const std::vector<std::set<int>>& inputs,
    size_t index,
    std::vector<std::set<int>>& buckets,
    size_t maxSize)
{
    if (index == inputs.size()) return true;

    const std::set<int>& current = inputs[index];

    for (size_t i = 0; i < buckets.size(); ++i) {
        // Optimisation: skip duplicate bucket states to avoid redundant branches.
        // If this bucket has the same contents as a previous one, we already
        // tried assigning to an identical bucket and it either worked or failed.
        bool isDuplicate = false;
        for (size_t j = 0; j < i; ++j) {
            if (buckets[j] == buckets[i]) {
                isDuplicate = true;
                break;
            }
        }
        if (isDuplicate) continue;

        std::set<int> merged = setUnion(buckets[i], current);
        if (merged.size() <= maxSize) {
            std::set<int> saved = buckets[i];
            buckets[i] = merged;
            if (backtrack(inputs, index + 1, buckets, maxSize))
                return true;
            buckets[i] = saved;
        }
    }

    return false;
}

std::vector<std::set<int>> reduceToNBuckets(
    std::vector<std::set<int>> inputs,
    size_t numBuckets,
    size_t maxSize)
{
    if (numBuckets < 2 || numBuckets > 4)
        throw std::invalid_argument("numBuckets must be between 2 and 4 inclusive");

    // Sort largest sets first to fail fast on infeasible cases
    std::sort(inputs.begin(), inputs.end(),
        [](const std::set<int>& a, const std::set<int>& b) {
            return a.size() > b.size();
        });

    std::vector<std::set<int>> buckets(numBuckets);

    if (!backtrack(inputs, 0, buckets, maxSize))
        throw std::runtime_error(
            "Cannot partition input sets into " + std::to_string(numBuckets) +
            " buckets of size <= " + std::to_string(maxSize));

    return buckets;
}