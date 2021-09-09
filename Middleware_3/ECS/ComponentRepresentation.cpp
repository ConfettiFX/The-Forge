/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

#include "ComponentRepresentation.h"
#include <string.h>
#include "../../Common_3/OS/Interfaces/IMemory.h" // NOTE: this should be the last include in a .cpp
using namespace FCR;

uint32_t const UniqueIdGenerator::generateUniqueId(eastl::string component_name, BaseComponent* (*func)())
{
	static uint32_t m_VarIdCounter = 0;
	eastl::hash < eastl::string > hashStr;
	BaseComponent::instertIntoComponentGeneratorMap((uint32_t)hashStr(component_name), func);
	return m_VarIdCounter++;
}

VarRepresentationsAndOrderedIds
FCR::ComponentRepresentationBuilder::create()
{
	return m_Tmp;
}

void FCR::ComponentRepresentationBuilder::addVariableRepresentation(eastl::string const& lookup, ComponentVariableId const id, ComponentVarRepresentation const& repr)
{
	m_Tmp.varNames[lookup] = id;
	m_Tmp.representation[id] = repr;
	m_Tmp.orderedIds.push_back(id);
}

FCR::CompVarType::~CompVarType()
{

}

FCR::CompVarType::CompVarType()
{

}

static bool approximatelyEqual(float a, float b, float epsilon)
{
	return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

bool FCR::BoolVar::operator==(const BoolVar &otherVar) const
{
	return otherVar.value == value;
}

void FCR::BoolVar::setValueFromComponent(void* const pVal)
{
	value = *(static_cast<bool*>(pVal));
}

FCR::BoolVar::BoolVar()
{
	value = false;
}

bool FCR::IntVar::operator==(const IntVar &otherVar) const
{
	return otherVar.value == value;
}

void FCR::IntVar::setValueFromComponent(void* const pVal)
{
	value = *(static_cast<int32_t*>(pVal));
}

FCR::IntVar::IntVar()
{
	value = 0;
}

bool FCR::UintVar::operator==(const UintVar &otherVar) const
{
	return otherVar.value == value;
}

void FCR::UintVar::setValueFromComponent(void* const pVal)
{
	value = *(static_cast<uint32_t*>(pVal));
}

FCR::UintVar::UintVar()
{
	value = 0u;
}

bool FCR::FloatVar::operator==(const FloatVar &otherVar) const
{
	const float EPSILON = 0.001f;
	return approximatelyEqual(otherVar.value, value, EPSILON);
}

void FCR::FloatVar::setValueFromComponent(void* const pVal)
{
	value = *(static_cast<float*>(pVal));
}

FCR::FloatVar::FloatVar()
{
	value = 0.f;
}

bool FCR::Float2Var::operator==(const Float2Var &otherVar) const
{
	const float EPSILON = 0.001f;
	return (
		approximatelyEqual(otherVar.value.getX(), value.getX(), EPSILON) &&
		approximatelyEqual(otherVar.value.getY(), value.getY(), EPSILON) );
}

void FCR::Float2Var::setValueFromComponent(void* const pVal)
{
	value = *(static_cast<float2*>(pVal));
}

FCR::Float2Var::Float2Var()
{
	value = float2(0.f, 0.f);
}

bool FCR::Float3Var::operator==(const Float3Var &otherVar) const
{
	const float EPSILON = 0.001f;
	return (
		approximatelyEqual(otherVar.value.getX(), value.getX(), EPSILON) &&
		approximatelyEqual(otherVar.value.getY(), value.getY(), EPSILON) &&
		approximatelyEqual(otherVar.value.getZ(), value.getZ(), EPSILON) );
}

void FCR::Float3Var::setValueFromComponent(void* const pVal)
{
	value = *(static_cast<float3*>(pVal));
}

FCR::Float3Var::Float3Var()
{
	value = float3(0.f, 0.f, 0.f);
}

bool FCR::Float4Var::operator==(const Float4Var &otherVar) const
{
	const float EPSILON = 0.001f;
	return (
		approximatelyEqual(otherVar.value.getX(), value.getX(), EPSILON) &&
		approximatelyEqual(otherVar.value.getY(), value.getY(), EPSILON) &&
		approximatelyEqual(otherVar.value.getZ(), value.getZ(), EPSILON) &&
		approximatelyEqual(otherVar.value.getW(), value.getW(), EPSILON) );
}

void FCR::Float4Var::setValueFromComponent(void* const pVal)
{
	value = *(static_cast<float4*>(pVal));
}

FCR::Float4Var::Float4Var()
{
	value = float4(0.f, 0.f, 0.f, 0.f);
}

bool FCR::TransformMatrixVar::operator==(const TransformMatrixVar &otherVar) const
{
	const Vector4 otherCols[4] =
	{
		otherVar.value.getCol0(),
		otherVar.value.getCol1(),
		otherVar.value.getCol2(),
		otherVar.value.getCol3(),
	};
	const Vector4 thisCols[4] =
	{
		value.getCol0(),
		value.getCol1(),
		value.getCol2(),
		value.getCol3(),
	};

	const float EPSILON = 0.001f;
	return (
		approximatelyEqual(otherCols[0].getX(), thisCols[0].getX(), EPSILON) &&
		approximatelyEqual(otherCols[0].getY(), thisCols[0].getY(), EPSILON) &&
		approximatelyEqual(otherCols[0].getZ(), thisCols[0].getZ(), EPSILON) &&
		approximatelyEqual(otherCols[0].getW(), thisCols[0].getW(), EPSILON) &&

		approximatelyEqual(otherCols[1].getX(), thisCols[1].getX(), EPSILON) &&
		approximatelyEqual(otherCols[1].getY(), thisCols[1].getY(), EPSILON) &&
		approximatelyEqual(otherCols[1].getZ(), thisCols[1].getZ(), EPSILON) &&
		approximatelyEqual(otherCols[1].getW(), thisCols[1].getW(), EPSILON) &&

		approximatelyEqual(otherCols[2].getX(), thisCols[2].getX(), EPSILON) &&
		approximatelyEqual(otherCols[2].getY(), thisCols[2].getY(), EPSILON) &&
		approximatelyEqual(otherCols[2].getZ(), thisCols[2].getZ(), EPSILON) &&
		approximatelyEqual(otherCols[2].getW(), thisCols[2].getW(), EPSILON) &&

		approximatelyEqual(otherCols[3].getX(), thisCols[3].getX(), EPSILON) &&
		approximatelyEqual(otherCols[3].getY(), thisCols[3].getY(), EPSILON) &&
		approximatelyEqual(otherCols[3].getZ(), thisCols[3].getZ(), EPSILON) &&
		approximatelyEqual(otherCols[3].getW(), thisCols[3].getW(), EPSILON) 
		);
}

void FCR::TransformMatrixVar::setValueFromComponent(void* const pVal)
{
	value = *(static_cast<mat4*>(pVal));
}

FCR::TransformMatrixVar::TransformMatrixVar()
{
	value = mat4::identity();
}

StringVar& FCR::StringVar::operator=(const StringVar& other)
{
	strcpy(value, other.value);

	return *this;
}

FCR::StringVar::StringVar(const StringVar& other)
{	
	strcpy(value, other.value);
}

bool FCR::StringVar::operator==(const StringVar &otherVar) const
{
	return strcmp(otherVar.value, value) == 0;
}

void FCR::StringVar::setValueFromComponent(void* const pVal)
{
	strcpy(value, static_cast<char*>(pVal));
}

FCR::StringVar::StringVar()
{
	value[0] = '\0';
}

FCR::GeometryVar::GeometryVar()
{

}

bool FCR::EnumVar::operator==(const EnumVar &otherVar) const
{
	return otherVar.value == value;
}

void FCR::EnumVar::setValueFromComponent(void* const pVal)
{
	value = *(static_cast<int32_t*>(pVal));
}

FCR::EnumVar::EnumVar()
{
	value = 0;
}
