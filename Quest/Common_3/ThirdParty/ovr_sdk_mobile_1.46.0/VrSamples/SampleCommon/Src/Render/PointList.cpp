/************************************************************************************

Filename    :   PointList.cpp
Content     :   Abstract base class for a list of points.
Created     :   6/16/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#include "PointList.h"
#include <assert.h>

using OVR::Vector3f;

namespace OVRFW {

//==============================================================================================
// ovrPointList_Vector

ovrPointList_Vector::ovrPointList_Vector(const int maxPoints) : MaxPoints(maxPoints) {}

int ovrPointList_Vector::GetNext(const int cur) const {
    int next = cur + 1;
    int arraySize = (int)Points.size();
    if (next >= arraySize) {
        return -1;
    }
    return next;
}

void ovrPointList_Vector::RemoveHead() {
    for (int i = 1; i < (int)Points.size(); ++i) {
        Points[i - 1] = Points[i];
    }
    Points.resize(Points.size() - 1);
}

//==============================================================================================
// ovrPointList_Circular

ovrPointList_Circular::ovrPointList_Circular(const int bufferSize)
    : BufferSize(bufferSize), CurPoints(0), HeadIndex(0), TailIndex(0) {
    Points.resize(BufferSize);
}

int ovrPointList_Circular::GetFirst() const {
    if (IsEmpty()) {
        assert(!IsEmpty());
        return -1;
    }
    return HeadIndex;
}

int ovrPointList_Circular::GetNext(const int cur) const {
    if (IsEmpty()) {
        assert(!IsEmpty());
        return -1;
    }
    int next = IncIndex(cur);
    if (next == TailIndex) {
        return -1;
    }
    return next;
}

int ovrPointList_Circular::GetLast() const {
    if (IsEmpty()) {
        return -1;
    }
    const int lastIndex = DecIndex(TailIndex);
    return lastIndex;
}

void ovrPointList_Circular::AddToTail(const Vector3f& p) {
    if (!IsFull()) {
        CurPoints++;
        Points[TailIndex] = p;
        TailIndex = IncIndex(TailIndex);
    }
}
void ovrPointList_Circular::RemoveHead() {
    if (HeadIndex == TailIndex) {
        return;
    }
    HeadIndex = IncIndex(HeadIndex);
    CurPoints--;
}

int ovrPointList_Circular::DecIndex(const int in) const {
    int d = in - 1;
    if (d < 0) {
        d += BufferSize;
    }
    return d;
}

} // namespace OVRFW
