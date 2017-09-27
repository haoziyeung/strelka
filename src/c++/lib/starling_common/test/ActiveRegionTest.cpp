//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2017 Illumina, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

/// \file
/// \author Sangtae Kim
///

#include "starling_base_options_test.hh"
#include "starling_common/ActiveRegionDetector.hh"
#include "starling_common/IndelBuffer.hh"


#include "boost/test/unit_test.hpp"



typedef std::unique_ptr<IndelBuffer> IndelBufferPtr;

struct TestIndelBuffer
{
    explicit
    TestIndelBuffer(
        const reference_contig_segment& ref)
    {
        // fake starling options
        _opt.is_user_genome_size = true;
        _opt.user_genome_size = ref.seq().size();

        const double maxDepth = 100.0;
        _doptPtr.reset(new starling_base_deriv_options(_opt));

        _IndelBufferPtr.reset(new IndelBuffer(_opt, *_doptPtr, ref));
        _IndelBufferPtr->registerSample(depth_buffer(), depth_buffer(), maxDepth);
        _IndelBufferPtr->finalizeSamples();

    }

    IndelBuffer&
    getIndelBuffer()
    {
        return *_IndelBufferPtr;
    }

private:
    starling_base_options_test _opt;
    std::unique_ptr<starling_base_deriv_options> _doptPtr;
    std::unique_ptr<IndelBuffer> _IndelBufferPtr;
};


BOOST_AUTO_TEST_SUITE( test_activeRegion )

// checks whether positions with consistent mismatches are marked as polymorphic sites
BOOST_AUTO_TEST_CASE( test_multiSampleMMDF )
{
    reference_contig_segment ref;
    ref.seq() = "GATCTGT";
    const unsigned maxIndelSize = 50;
    const int sampleCount = 3;
    const int depth = 50;

    TestIndelBuffer testBuffer(ref);
    CandidateSnvBuffer testSnvBuffer(sampleCount);

    ActiveRegionDetector activeRegionDetector(ref, testBuffer.getIndelBuffer(),
                                              testSnvBuffer, maxIndelSize, sampleCount, false);

    const auto snvPos = std::set<pos_t>({2, 4, 5});

    pos_t refLength = (pos_t)ref.seq().length();

    // fake reading reads
    for (int alignId=0; alignId < depth*sampleCount; ++alignId)
    {
        unsigned sampleIndex = alignId % sampleCount;
        bool isForwardStrand = ((alignId % 4) == 0) or ((alignId % 4) == 3);
        activeRegionDetector.getReadBuffer(sampleIndex).setAlignInfo(alignId, sampleIndex, INDEL_ALIGN_TYPE::GENOME_TIER1_READ, isForwardStrand);
        for (pos_t pos(0); pos<refLength; ++pos)
        {
            // only sample 1 has mismatches
            // alternative allele frequency 0.5
            if (sampleIndex != 1 or ((alignId/sampleCount) % 2)
                or (snvPos.find(pos) == snvPos.end()))
            {
                // No SNV
                activeRegionDetector.getReadBuffer(sampleIndex).insertMatch(alignId, pos);
            }
            else
            {
                // SNV position
                activeRegionDetector.getReadBuffer(sampleIndex).insertMismatch(alignId, pos, 'A');
            }
        }
    }

    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        for (pos_t pos(0); pos<refLength; ++pos)
        {
            activeRegionDetector.updateEndPosition(pos);
        }
        activeRegionDetector.clear();
    }

    // check if isCandidateSnv are correctly set
    for (unsigned sampleIndex(0); sampleIndex<sampleCount; ++sampleIndex)
    {
        for (pos_t pos(0); pos<refLength; ++pos)
        {
            if (snvPos.find(pos) != snvPos.end())
            {
                // SNV
                BOOST_REQUIRE_EQUAL(testSnvBuffer.isCandidateSnvAnySample(pos, 'A'), true);
            }
            else
            {
                // No SNV
                BOOST_REQUIRE_EQUAL(testSnvBuffer.isCandidateSnvAnySample(pos, 'A'), false);
            }
        }
    }
}

// Checks whether an indel is correctly confirmed in active regions
BOOST_AUTO_TEST_CASE( test_indelCandidacy )
{
    reference_contig_segment ref;
    ref.seq() = "TCTCT";

    const unsigned maxIndelSize = 50;
    const int sampleCount = 1;
    const unsigned sampleIndex = 0;

    TestIndelBuffer testBuffer(ref);
    CandidateSnvBuffer testSnvBuffer(sampleCount);

    ActiveRegionDetector detector(ref, testBuffer.getIndelBuffer(),
                                  testSnvBuffer, maxIndelSize, sampleCount, false);

    const int depth = 50;

    const pos_t indelPos = 2;
    auto indelKey = IndelKey(indelPos, INDEL::INDEL, 0, "AG");

    pos_t refLength = (pos_t)ref.seq().length();

    // fake reading reads
    for (int alignId=0; alignId < depth; ++alignId)
    {
        bool isForwardStrand = ((alignId % 4) == 0) or ((alignId % 4) == 3);
        detector.getReadBuffer(sampleIndex).setAlignInfo(alignId, sampleIndex, INDEL_ALIGN_TYPE::GENOME_TIER1_READ, isForwardStrand);
        for (pos_t pos(0); pos<refLength; ++pos)
        {
            detector.getReadBuffer(sampleIndex).insertMatch(alignId, pos);

            if (pos == indelPos && (alignId % 2))
            {
                IndelObservation indelObservation;

                IndelObservationData indelObservationData;
                indelObservation.key = indelKey;
                indelObservationData.id = alignId;
                indelObservationData.iat = INDEL_ALIGN_TYPE::GENOME_TIER1_READ;
                indelObservation.data = indelObservationData;

                detector.getReadBuffer(sampleIndex).insertIndel(indelObservation);
            }
        }
    }

    for (pos_t pos(0); pos<refLength; ++pos)
    {
        detector.updateEndPosition(pos);
    }
    detector.clear();

    const auto itr(testBuffer.getIndelBuffer().getIndelIter(indelKey));
    BOOST_REQUIRE_EQUAL(itr->second.isConfirmedInActiveRegion, true);
}


BOOST_AUTO_TEST_CASE( test_jumpingPositions )
{
    const pos_t startPositions[] = {100, 7000};
    const pos_t snvOffsets[] = {40, 42};

    std::string refSeq(10000, 'A');
    for (auto startPosition : startPositions)
    {
        for (auto snvOffset : snvOffsets)
        {
            // to avoid hpol filter
            refSeq[startPosition+snvOffset - 1] = 'T';
            refSeq[startPosition+snvOffset + 1] = 'T';
        }
    }

    reference_contig_segment ref;
    ref.seq() = refSeq;

    const unsigned maxIndelSize = 50;
    const int sampleCount = 1;
    const unsigned sampleIndex = 0;

    TestIndelBuffer testBuffer(ref);
    CandidateSnvBuffer testSnvBuffer(sampleCount);

    ActiveRegionDetector detector(ref, testBuffer.getIndelBuffer(),
                                  testSnvBuffer, maxIndelSize, sampleCount, false);

    // fake reading reads
    const int depth = 50;
    const unsigned readLength = 100;


    for (auto startPosition : startPositions)
    {
        pos_t endPosition = startPosition + readLength;
        for (int alignId=0; alignId < depth; ++alignId)
        {
            bool isForwardStrand = ((alignId % 4) == 0) or ((alignId % 4) == 3);
            detector.getReadBuffer(sampleIndex).setAlignInfo(alignId, sampleIndex, INDEL_ALIGN_TYPE::GENOME_TIER1_READ, isForwardStrand);
            for (pos_t pos(startPosition); pos<endPosition; ++pos)
            {
                // SNVs at startPosition+10 and startPosition+12
                auto isSnvPosition = (pos == startPosition+snvOffsets[0]) || (pos == startPosition+snvOffsets[1]);
                if ((alignId % 2) && isSnvPosition)
                {
                    detector.getReadBuffer(sampleIndex).insertMismatch(alignId, pos, 'G');
                }
                else
                {
                    detector.getReadBuffer(sampleIndex).insertMatch(alignId, pos);
                }
            }
        }

        for (pos_t pos(startPosition); pos<endPosition; ++pos)
        {
            detector.updateEndPosition(pos);
        }
        detector.clear();

        // check if isCandidateSnv are correctly set
        BOOST_REQUIRE_EQUAL(testSnvBuffer.isCandidateSnv(sampleIndex, startPosition + snvOffsets[0], 'G'), true);
        BOOST_REQUIRE_EQUAL(testSnvBuffer.isCandidateSnv(sampleIndex, startPosition + snvOffsets[1], 'G'), true);
    }
}

// Checks whether an indel is left-shifted
BOOST_AUTO_TEST_CASE( test_leftShiftIndel )
{
    reference_contig_segment ref;
    ref.seq() = "GTCC";

    const unsigned maxIndelSize = 50;
    const unsigned sampleCount = 1;
    const unsigned sampleIndex = 0;

    TestIndelBuffer testBuffer(ref);
    CandidateSnvBuffer testSnvBuffer(sampleCount);

    ActiveRegionDetector detector(ref, testBuffer.getIndelBuffer(),
                                  testSnvBuffer, maxIndelSize, sampleCount, false);

    const int depth = 50;

    const pos_t indelPos = 2;
    auto indelKey = IndelKey(indelPos, INDEL::INDEL, 0, "ATAT");

    pos_t refLength = (pos_t)ref.seq().length();

    // fake reading reads
    for (int alignId=0; alignId < depth; ++alignId)
    {
        bool isForwardStrand = ((alignId % 4) == 0) or ((alignId % 4) == 3);
        detector.getReadBuffer(sampleIndex).setAlignInfo(alignId, sampleIndex, INDEL_ALIGN_TYPE::GENOME_TIER1_READ, isForwardStrand);
        for (pos_t pos(0); pos<refLength; ++pos)
        {
            detector.getReadBuffer(sampleIndex).insertMatch(alignId, pos);

            if (pos == indelPos && (alignId % 2))
            {
                IndelObservation indelObservation;

                IndelObservationData indelObservationData;
                indelObservation.key = indelKey;
                indelObservationData.id = alignId;
                indelObservationData.iat = INDEL_ALIGN_TYPE::GENOME_TIER1_READ;
                indelObservation.data = indelObservationData;

                detector.getReadBuffer(sampleIndex).insertIndel(indelObservation);
            }
        }
    }

    for (pos_t pos(0); pos<refLength; ++pos)
    {
        detector.updateEndPosition(pos);
    }
    detector.clear();

    // check if the indel is shifted 1 base to the left
    auto leftShiftedIndelKey = IndelKey(indelPos-1, INDEL::INDEL, 0, "TATA");
    const auto itr(testBuffer.getIndelBuffer().getIndelIter(leftShiftedIndelKey));
    BOOST_REQUIRE_EQUAL(itr->second.isConfirmedInActiveRegion, true);
}

BOOST_AUTO_TEST_CASE( test_selectingManyHaplotypes )
{
    reference_contig_segment ref;
    ref.seq() = "GATCTGT";
    const unsigned maxIndelSize(50);
    const unsigned sampleCount(1);
    const unsigned sampleIndex(0);
    const unsigned depth(50);

    TestIndelBuffer testBuffer(ref);
    CandidateSnvBuffer testSnvBuffer(sampleCount);

    unsigned defaultPloidy(3);
    ActiveRegionDetector activeRegionDetector(ref, testBuffer.getIndelBuffer(),
                                              testSnvBuffer, maxIndelSize, sampleCount, false, defaultPloidy);

    const auto snvPos = std::set<pos_t>({2, 4});

    pos_t refLength = (pos_t)ref.seq().length();

    // create 4 haplotypes with differing bases at positions 2 and 4
    // hap0 (no SNV): 20 reads => selected
    // hap1 (SNV at 2): 13 haplotypes => selected
    // hap2 (SNV at 4): 12 haplotypes => selected
    // hap3 (SNV at 6): 5 haplotypes => not selected
    for (unsigned alignId(0); alignId < depth; ++alignId)
    {
        bool isForwardStrand = ((alignId % 2) == 0);
        activeRegionDetector.getReadBuffer(sampleIndex).setAlignInfo(
                alignId, sampleIndex, INDEL_ALIGN_TYPE::GENOME_TIER1_READ, isForwardStrand);

        bool isSnvAtPos2(false);
        bool isSnvAtPos4(false);
        bool isSnvAtPos6(false);
        if (alignId < 20)
        {
            // no SNV
        }
        else if (alignId < 33)
        {
            isSnvAtPos2 = true;
        }
        else if (alignId < 45)
        {
            isSnvAtPos4 = true;
        }
        else
        {
            isSnvAtPos6 = true;
        }

        for (pos_t pos(0); pos<refLength; ++pos)
        {

            if ((isSnvAtPos2 && (pos == 2))
                    || (isSnvAtPos4 && (pos == 4))
                    || (isSnvAtPos6 && (pos == 6)))
            {
                // SNV position
                activeRegionDetector.getReadBuffer(sampleIndex).insertMismatch(alignId, pos, 'A');
            }
            else
            {
                // No SNV
                activeRegionDetector.getReadBuffer(sampleIndex).insertMatch(alignId, pos);
            }
        }
    }

    // Create and process active regions
    for (pos_t pos(0); pos<refLength; ++pos)
    {
        activeRegionDetector.updateEndPosition(pos);
    }
    activeRegionDetector.clear();

    // check if isCandidateSnv are correctly set
    for (pos_t pos(0); pos<refLength; ++pos)
    {
        // pos 2 and 4 must be candidate SNV positions
        // pos 6 must not be because hap3 was not selected
        if ((pos == 2 || pos == 4))
        {
            // SNV
            BOOST_REQUIRE_EQUAL(testSnvBuffer.isCandidateSnvAnySample(pos, 'A'), true);
        }
        else
        {
            // No SNV
            BOOST_REQUIRE_EQUAL(testSnvBuffer.isCandidateSnvAnySample(pos, 'A'), false);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
