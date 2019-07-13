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
	const float EPSILON = 0.001f;
	return (
		approximatelyEqual(otherVar.value.getCol0().getX(), value.getCol0().getX(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol0().getY(), value.getCol0().getY(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol0().getZ(), value.getCol0().getZ(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol0().getW(), value.getCol0().getW(), EPSILON) &&

		approximatelyEqual(otherVar.value.getCol1().getX(), value.getCol1().getX(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol1().getY(), value.getCol1().getY(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol1().getZ(), value.getCol1().getZ(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol1().getW(), value.getCol1().getW(), EPSILON) &&

		approximatelyEqual(otherVar.value.getCol2().getX(), value.getCol2().getX(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol2().getY(), value.getCol2().getY(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol2().getZ(), value.getCol2().getZ(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol2().getW(), value.getCol2().getW(), EPSILON) &&

		approximatelyEqual(otherVar.value.getCol3().getX(), value.getCol3().getX(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol3().getY(), value.getCol3().getY(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol3().getZ(), value.getCol3().getZ(), EPSILON) &&
		approximatelyEqual(otherVar.value.getCol3().getW(), value.getCol3().getW(), EPSILON) 
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
