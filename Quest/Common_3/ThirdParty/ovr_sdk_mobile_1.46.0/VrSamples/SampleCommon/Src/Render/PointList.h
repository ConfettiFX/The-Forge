/************************************************************************************

Filename    :   PointList.h
Content     :   Abstract base class for a list of points.
Created     :   6/16/2017
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/

#pragma once

#include <cstdint>
#include <vector>
#include "OVR_Math.h"

namespace OVRFW {

//==============================================================
// ovrPointList
class ovrPointList {
   public:
    virtual ~ovrPointList() {}

    virtual bool IsEmpty() const = 0;
    virtual bool IsFull() const = 0;
    virtual int GetFirst() const = 0;
    virtual int GetNext(const int cur) const = 0;
    virtual int GetLast() const = 0;
    virtual int GetCurPoints() const = 0;
    virtual int GetMaxPoints() const = 0;

    virtual const OVR::Vector3f& Get(const int index) const = 0;
    virtual OVR::Vector3f& Get(const int index) = 0;

    virtual void AddToTail(const OVR::Vector3f& p) = 0;
    virtual void RemoveHead() = 0;
};

//==============================================================
// ovrPointList_Vector
class ovrPointList_Vector : public ovrPointList {
   public:
    ovrPointList_Vector(const int maxPoints);
    virtual ~ovrPointList_Vector() {}

    virtual bool IsEmpty() const override {
        return Points.size() == 0;
    }
    virtual bool IsFull() const override {
        return false;
    }
    virtual int GetFirst() const override {
        return 0;
    }
    virtual int GetNext(const int cur) const override;
    virtual int GetLast() const override {
        return (int)Points.size() - 1;
    }
    virtual int GetCurPoints() const override {
        return Points.size();
    }
    virtual int GetMaxPoints() const override {
        return MaxPoints;
    }

    virtual const OVR::Vector3f& Get(const int index) const override {
        return Points[index];
    }
    virtual OVR::Vector3f& Get(const int index) override {
        return Points[index];
    }

    virtual void AddToTail(const OVR::Vector3f& p) override {
        Points.push_back(p);
    }
    virtual void RemoveHead() override;

   private:
    std::vector<OVR::Vector3f> Points;
    int MaxPoints;
};

//==============================================================
// ovrPointList_Circular
class ovrPointList_Circular : public ovrPointList {
   public:
    ovrPointList_Circular(const int bufferSize);
    virtual ~ovrPointList_Circular() {}

    virtual bool IsEmpty() const override {
        return HeadIndex == TailIndex;
    }
    virtual bool IsFull() const override {
        return IncIndex(TailIndex) == HeadIndex;
    }
    virtual int GetFirst() const override;
    virtual int GetNext(const int cur) const override;
    virtual int GetLast() const override;
    virtual int GetCurPoints() const override {
        return CurPoints;
    }
    virtual int GetMaxPoints() const override {
        return BufferSize - 1;
    }

    virtual const OVR::Vector3f& Get(const int index) const override {
        return Points[index];
    }
    virtual OVR::Vector3f& Get(const int index) override {
        return Points[index];
    }

    virtual void AddToTail(const OVR::Vector3f& p) override;
    virtual void RemoveHead() override;

   private:
    std::vector<OVR::Vector3f> Points;

    int BufferSize;
    int CurPoints;
    int HeadIndex;
    int TailIndex;

   private:
    int IncIndex(const int in) const {
        return (in + 1) % BufferSize;
    }
    int DecIndex(const int in) const;
};

} // namespace OVRFW
