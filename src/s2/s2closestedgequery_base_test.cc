// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#include "s2/s2closestedgequery_base.h"

#include <gtest/gtest.h>
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2edge_distances.h"
#include "s2/s2textformat.h"

namespace {

// This is a proof-of-concept prototype of a possible S2FurthestEdgeQuery
// class.  The purpose of this test is just to make sure that the code
// compiles and does something reasonable.  (A real implementation would need
// to be more careful about error bounds, it would implement a greater range
// of target types, etc.)
//
// It is based on the principle that for any two geometric objects X and Y on
// the sphere,
//
//     max_dist(X, Y) = Pi - min_dist(-X, Y)
//
// where "-X" denotes the reflection of X through the origin (i.e., to the
// opposite side of the sphere).

// MaxDistance is a class that allows maximum distances to be computed using a
// minimum distance algorithm.  It essentially treats a distance "x" as the
// supplementary distance (Pi - x).
class MaxDistance {
 public:
  MaxDistance() : distance_() {}
  explicit MaxDistance(S1ChordAngle x) : distance_(x) {}
  explicit operator S1ChordAngle() const { return distance_; }
  static MaxDistance Zero() {
    return MaxDistance(S1ChordAngle::Straight());
  }
  static MaxDistance Infinity() {
    return MaxDistance(S1ChordAngle::Negative());
  }
  static MaxDistance Negative() {
    return MaxDistance(S1ChordAngle::Infinity());
  }
  friend bool operator==(MaxDistance x, MaxDistance y) {
    return x.distance_ == y.distance_;
  }
  friend bool operator<(MaxDistance x, MaxDistance y) {
    return x.distance_ > y.distance_;
  }
  friend MaxDistance operator-(MaxDistance x, MaxDistance y) {
    return MaxDistance(x.distance_ + y.distance_);
  }
  static S1Angle GetAngleBound(MaxDistance x) {
    return (S1ChordAngle::Straight() - x.distance_).ToAngle();
  }

 private:
  S1ChordAngle distance_;
};

using FurthestEdgeQuery = S2ClosestEdgeQueryBase<MaxDistance>;

class FurthestPointTarget : public FurthestEdgeQuery::Target {
 public:
  explicit FurthestPointTarget(S2Point const& point) : point_(point) {}

  int max_brute_force_edges() const override { return 100; }

  S2Cap GetCapBound() const override {
    return S2Cap(-point_, S1ChordAngle::Zero());
  }

  bool UpdateMinDistance(S2Point const& v0, S2Point const& v1,
                         MaxDistance* min_dist) const override {
    S1ChordAngle dist180 =
        S1ChordAngle(*min_dist).is_negative() ? S1ChordAngle::Infinity() :
        S1ChordAngle::Straight() - S1ChordAngle(*min_dist);
    if (!S2::UpdateMinDistance(-point_, v0, v1, &dist180)) return false;
    *min_dist = MaxDistance(S1ChordAngle::Straight() - dist180);
    return true;
  }

  bool UpdateMinDistance(S2Cell const& cell,
                         MaxDistance* min_dist) const override {
    MaxDistance dist(S1ChordAngle::Straight() - cell.GetDistance(-point_));
    if (*min_dist < dist) return false;
    *min_dist = dist;
    return true;
  }

 private:
  S2Point point_;
};

TEST(S2ClosestEdgeQueryBase, MaxDistance) {
  auto index = s2textformat::MakeIndex("0:0 | 1:0 | 2:0 | 3:0 # #");
  FurthestEdgeQuery query(index.get());
  FurthestPointTarget target(s2textformat::MakePoint("4:0"));
  FurthestEdgeQuery::Options options;
  options.set_max_edges(1);
  auto results = query.FindClosestEdges(target, options);
  ASSERT_EQ(1, results.size());
  EXPECT_EQ(0, results[0].shape_id);
  EXPECT_EQ(0, results[0].edge_id);
  EXPECT_NEAR(4, S1ChordAngle(results[0].distance).ToAngle().degrees(), 1e-13);
}

}  // namespace
