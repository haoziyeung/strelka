// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2016 Illumina, Inc.
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
/*
 *      Author: Morten Kallberg
 */

#pragma once

#include "json/json.h"

#include <map>
#include <string>


/// parse common meta-data format shared for all variant scoring models
///
struct VariantScoringModelMetadata
{
    typedef std::map<std::string,unsigned> featureMap_t;

    VariantScoringModelMetadata() {}

    void Deserialize(
        const featureMap_t& featureMap,
        const Json::Value& root);

    std::string date;
    std::string ModelType;

    /// as part of an optional calibration component to all models, scale the raw prob using this factor before reporting:
    double probScale = 1.;

    /// \TODO Doc this. what is the orientation of this number? <,>,<=,>=? Does it mean filter stuff to remove or to keep?
    double filterCutoff;
};

