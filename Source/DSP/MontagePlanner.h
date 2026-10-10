/*
    MontagePlanner.h — turns a finished take into an edit map.

    The warning during recording saves seconds. This saves hours. After the take,
    the planner walks the phrase list and answers the three questions that every
    podcast edit actually consists of:

      1. Which of these phrases was said more than once, and which attempt is the
         keeper?
      2. Which phrases are unusable on their own terms (clipped, plosive,
         abandoned) and have no replacement?
      3. Where are the dead gaps, and how much of each should go?

    Question 1 is answered with the spectro-temporal fingerprint from the core,
    not with timing heuristics. Two takes of the same sentence look alike in the
    12x16 band/time grid whatever the pauses around them were.

    Nothing here is destructive and nothing here is guessed silently: every
    decision carries a reason string and a number, so the plan can be read and
    overruled.
*/

#pragma once

#include "TakeWatchCore.h"
#include <vector>
#include <string>

namespace tw
{

enum class Action { Keep, CutRetake, CutBad, TrimGap };

struct EditItem
{
    Action action     = Action::Keep;
    double startSec   = 0.0;
    double endSec     = 0.0;
    int    phraseIndex = -1;
    float  score      = 0.0f;      ///< similarity for retakes, severity for bad
    std::string reason;            ///< Polish, shown verbatim in the UI
};

struct MontageOptions
{
    float  retakeSimilarity = 0.70f;  ///< absolute floor, below which never a match
    float  retakeMargin     = 0.18f;  ///< how far above this session's background
    double retakeMaxGap     = 8.0;    ///< seconds between attempts
    double durationRatio    = 0.32f;  ///< attempts must be within this much in length
    double gapKeep          = 0.45;   ///< silence left between kept phrases
    double gapTrimAbove     = 1.10;   ///< only trim gaps longer than this
    bool   cutUnreplacedBad = false;  ///< cut a bad phrase even with no retake

    /** The safety catch, and the reason this planner can be trusted with a
        session. A repeat is only ever cut when the earlier attempt was already
        flagged during recording — a plosive, a clip, an abandoned start. Two
        sentences that merely resemble each other are both kept, always. Turn
        this off only for material you know is take-after-take. */
    bool   onlyCutFlagged   = true;
};

struct MontagePlan
{
    std::vector<EditItem> items;
    double originalSec = 0.0;
    double keptSec     = 0.0;
    int    retakesCut  = 0;
    int    badCut      = 0;
    double gapsTrimmed = 0.0;
};

//==============================================================================
inline MontagePlan planMontage (const TakeWatch& tw,
                                const MontageOptions& opt = {})
{
    MontagePlan plan;
    const int n = tw.numPhrases();
    if (n == 0) return plan;

    std::vector<int> group (n, -1);   // retake group id per phrase
    std::vector<char> cut (n, 0);

    // ---- background similarity ---------------------------------------------
    //
    // "How alike is 0.90?" has no answer in the abstract. It depends on the
    // voice, the room and how varied the material is: a person reading a list of
    // numbers produces phrases that resemble each other far more than a person
    // telling a story does. So the threshold is measured, not assumed — every
    // phrase is compared against distant phrases in the same take, and a match
    // has to stand clear of that background before it counts as a repeat.
    std::vector<float> background ((size_t) n, 0.0f);

    for (int i = 0; i < n; ++i)
    {
        float samples[64];
        int   count = 0;

        for (int j = 0; j < n && count < 64; ++j)
        {
            if (std::abs (i - j) < 6) continue;             // near neighbours may be real repeats
            const float sim = similarity (tw.phrase (i), tw.phrase (j));
            if (sim > 0.0f) samples[count++] = sim;
        }

        if (count >= 4)
        {
            std::nth_element (samples, samples + count * 3 / 4, samples + count);
            background[(size_t) i] = samples[count * 3 / 4];   // 75th percentile of "unrelated"
        }
        else
            background[(size_t) i] = 0.55f;                    // too short a take to measure
    }

    // ---- group retakes ------------------------------------------------------
    int nextGroup = 0;

    for (int i = 0; i < n; ++i)
    {
        if (group[i] >= 0) continue;
        group[i] = nextGroup;

        // Only look forward across a small window: a retake follows its attempt.
        // Three conditions have to hold together, because any one of them alone
        // produces false matches on ordinary speech.
        for (int j = i + 1; j < n && j <= i + 4; ++j)
        {
            if (group[j] >= 0) continue;
            if (tw.phrase(j).startSec - tw.phrase(j - 1).endSec > opt.retakeMaxGap) break;

            const auto& a = tw.phrase (i);
            const auto& b = tw.phrase (j);

            if (opt.onlyCutFlagged && ! a.flagged) continue;

            const double da = a.endSec - a.startSec;
            const double db = b.endSec - b.startSec;
            if (da < 0.1 || db < 0.1) continue;
            if (std::abs (da - db) / std::max (da, db) > opt.durationRatio) continue;

            const float sim  = similarity (a, b);
            const float bar  = std::max (opt.retakeSimilarity,
                                         background[(size_t) i] + opt.retakeMargin);

            if (sim >= bar)
                group[j] = nextGroup;
        }

        ++nextGroup;
    }

    // ---- pick a keeper per group -------------------------------------------
    for (int g = 0; g < nextGroup; ++g)
    {
        int members[8];
        int count = 0;
        for (int i = 0; i < n && count < 8; ++i)
            if (group[i] == g) members[count++] = i;

        if (count < 2) continue;

        // prefer the last clean attempt; if none is clean, the last one at all
        int keeper = members[count - 1];
        for (int k = count - 1; k >= 0; --k)
            if (! tw.phrase (members[k]).flagged) { keeper = members[k]; break; }

        for (int k = 0; k < count; ++k)
            if (members[k] != keeper) cut[members[k]] = 1;
    }

    // ---- build the plan -----------------------------------------------------
    double prevKeptEnd = -1.0;

    for (int i = 0; i < n; ++i)
    {
        const auto& p = tw.phrase (i);
        plan.originalSec = std::max (plan.originalSec, p.endSec);

        const bool bad = p.flagged
                         && (opt.cutUnreplacedBad || cut[i] != 0)
                         && p.verdict >= (int) Severity::Redo;

        if (cut[i] != 0 || (bad && opt.cutUnreplacedBad))
        {
            EditItem e;
            e.action      = cut[i] ? Action::CutRetake : Action::CutBad;
            e.startSec    = p.startSec;
            e.endSec      = p.endSec;
            e.phraseIndex = i;

            if (cut[i])
            {
                // report against the keeper for context
                int keeper = i;
                for (int j = i + 1; j < n; ++j)
                    if (group[j] == group[i]) { keeper = j; break; }

                e.score  = similarity (p, tw.phrase (keeper));
                e.reason = "powtorzone pozniej (podobienstwo "
                         + std::to_string ((int) std::round (e.score * 100.0f)) + "%)";
                ++plan.retakesCut;
            }
            else
            {
                e.score  = (float) p.verdict;
                e.reason = "fraza oznaczona jako zla, bez lepszej wersji";
                ++plan.badCut;
            }

            plan.items.push_back (std::move (e));
            continue;
        }

        // gap before this kept phrase
        if (prevKeptEnd > 0.0)
        {
            const double gap = p.startSec - prevKeptEnd;
            if (gap > opt.gapTrimAbove)
            {
                EditItem t;
                t.action   = Action::TrimGap;
                t.startSec = prevKeptEnd + opt.gapKeep * 0.5;
                t.endSec   = p.startSec  - opt.gapKeep * 0.5;
                t.score    = (float) (t.endSec - t.startSec);
                t.reason   = "cisza " + std::to_string ((int) std::round (gap * 10) / 10.0)
                           + " s -> zostaw " + std::to_string (opt.gapKeep) + " s";
                plan.gapsTrimmed += std::max (0.0, t.endSec - t.startSec);
                plan.items.push_back (std::move (t));
            }
        }

        EditItem k;
        k.action      = Action::Keep;
        k.startSec    = p.startSec;
        k.endSec      = p.endSec;
        k.phraseIndex = i;
        k.score       = p.flagged ? (float) p.verdict : 0.0f;
        if (p.flagged) k.reason = "zostawione mimo uwagi - brak lepszej wersji";
        plan.items.push_back (std::move (k));

        plan.keptSec += p.endSec - p.startSec;
        prevKeptEnd = p.endSec;
    }

    return plan;
}

} // namespace tw
