#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "LineLog.h"

namespace tw
{

/** Sample-domain replacement of [from,to). Outside the replaced interval,
    audio is copied bit-for-bit. The replacement fades across its first and
    last 10 ms using the adjacent original samples as the other side of each
    crossfade. */
struct ReplaceRange
{
    /** Convert the same time interval to rounded sample boundaries everywhere
        (round(to-from) can differ by one sample from round(to)-round(from)). */
    static bool sampleBounds (size_t sourceSamples, double sampleRate, double from, double to,
                              size_t& first, size_t& last)
    {
        if (! std::isfinite (sampleRate) || sampleRate <= 0.0
            || ! std::isfinite (from) || ! std::isfinite (to)
            || from < 0.0 || to <= from || to > (double) sourceSamples / sampleRate)
            return false;

        first = std::min (sourceSamples, (size_t) std::llround (from * sampleRate));
        last = std::min (sourceSamples, (size_t) std::llround (to * sampleRate));
        return last > first;
    }

    static bool render (const std::vector<float>& original,
                       const std::vector<float>& replacement,
                       double sampleRate, double from, double to,
                       bool fixedLength, std::vector<float>& out)
    {
        size_t a = 0, b = 0;
        if (! sampleBounds (original.size(), sampleRate, from, to, a, b)
            || replacement.empty() || b - a == original.size()) return false;
        if (fixedLength && replacement.size() != b - a) return false;

        const auto fade = std::min ({ (size_t) std::llround (sampleRate * 0.010),
                                      replacement.size() / 2,
                                      a == 0 ? replacement.size() : (size_t) (sampleRate * 0.010),
                                      b == original.size() ? replacement.size() : (size_t) (sampleRate * 0.010) });
        std::vector<float> result;
        result.reserve (original.size() - (b - a) + replacement.size());
        result.insert (result.end(), original.begin(), original.begin() + (long) a);

        for (size_t i = 0; i < replacement.size(); ++i)
        {
            float value = replacement[i];
            if (fade > 0 && i < fade && a > 0)
            {
                const float t = (float) (i + 1) / (float) (fade + 1);
                value = original[a - 1] * (1.0f - t) + value * t;
            }
            if (fade > 0 && replacement.size() - i <= fade && b < original.size())
            {
                const float t = (float) (replacement.size() - i) / (float) (fade + 1);
                value = value * t + original[b] * (1.0f - t);
            }
            result.push_back (value);
        }

        result.insert (result.end(), original.begin() + (long) b, original.end());
        out = std::move (result);
        return true;
    }

    /** Keep old marks before the replaced interval, place fresh marks inside
        it, and shift the remaining marks by the duration difference. */
    static std::vector<LineMark> remapMarks (const std::vector<LineMark>& oldMarks,
                                             const std::vector<LineMark>& newMarks,
                                             double from, double to,
                                             double replacementSeconds)
    {
        std::vector<LineMark> out;
        const double delta = replacementSeconds - (to - from);
        for (const auto& mark : oldMarks)
            if (mark.sec < from) out.push_back (mark);

        for (auto mark : newMarks)
        {
            if (mark.sec < 0.0 || mark.sec >= replacementSeconds) continue;
            mark.sec += from;
            mark.cutFrom = mark.cutTo = -1.0;
            mark.cutByHand = false;
            out.push_back (std::move (mark));
        }

        for (auto mark : oldMarks)
            if (mark.sec >= to)
            {
                mark.sec += delta;
                out.push_back (std::move (mark));
            }

        std::stable_sort (out.begin(), out.end(), [] (const LineMark& a, const LineMark& b) {
            return a.sec < b.sec;
        });
        return out;
    }
};

} // namespace tw
