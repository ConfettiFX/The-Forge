#pragma once

#include <stdarg.h>  // for va_* stuff
#include "BaseComponent.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../Common_3/OS/Math/MathTypes.h"

#include "../../Common_3/OS/Interfaces/ILog.h"
#undef alignment

#ifdef METAL
#ifndef strcpy_s
#define strcpy_s(dst, sz, src) strncpy(dst, src, sz)
#endif
#endif


/* ComponentRepresentation:
 * Allows to automate modifiable variables with outside systems such as UI.
 * Makes heavy usage of MACROS!
 *
 * Easiest way to understand usage is to check out a component that has a representation already created.
 */
//Important!!: Don't forget to bump up the version number in GladiatorIO.h if you modified any ComponentRepresentation
//Used to concatenate two var's names together using a "." between them
#define STRCAT_NX(a, b, c) a ## b ## c
#define STRCAT(a, b) STRCAT_NX(a, ".", c)

typedef uint32_t ComponentVariableId;

namespace FCR // The Forge Component Representation since there are name clashes with framework
{

enum ComponentVarType
{
	BOOL,
	INT,
	UINT,
	FLOAT,
	FLOAT2,
	FLOAT3,
	FLOAT4,
	TRANSFORM_MATRIX,	// MAT4
	STRING,				// eastl::string
	
	// Resources are eastl::strings ////////////	
	TEXTURE_2D,
	TEXTURE_CUBEMAP,
	MATERIAL,
    GEOMETRY,
    DOCUMENT,
	//////////////////////////////////////////////

	ENUM,				// int32_t
	NONE,				// type not assigned
};

enum ComponentVarAccess
{
	READ_ONLY,
	READ_WRITE,
};

struct ComponentVarRepresentation
{
	ComponentVarRepresentation()
	{
		name = "ERROR: BAD_VARIABLE!!!!!!";
		type = INT;
		access = READ_ONLY;
		min = max = step = 0.f;
		hasPerComponentFormat = false;
		perComponentFormat = "";
	}

	ComponentVarRepresentation(
		eastl::string const& name, 
		ComponentVarType const type, 
		ComponentVarAccess const access)
	{
		this->name = name;
		this->type = type;
		this->access = access;
		this->min = 0.f;
		this->max = 0.f;
		this->step = 0.f;
	}

	ComponentVarRepresentation(
		eastl::string const& name, 
		ComponentVarType const type, 
		ComponentVarAccess const access,
		eastl::unordered_map<int32_t, eastl::string> const& valueRepresentations)
	{
		this->name = name;
		this->type = type;
		this->access = access;
		this->valueRepresentations = valueRepresentations;
		this->min = 0.f;
		this->max = 0.f;
		this->step = 0.f;
	}

	eastl::string			name;
	ComponentVarType		type;
	ComponentVarAccess		access;

	eastl::unordered_map<int32_t, eastl::string> valueRepresentations; // for things like enums... may be used with other types in the future

	float min, max, step; // some types require min, max, and step values

	bool hasPerComponentFormat;
	eastl::string perComponentFormat; // delimited by ';'
};

class UniqueIdGenerator
{
public:
	// Used by MACRO and helper functions
	static uint32_t const
		generateUniqueId(eastl::string component_name, BaseComponent* (*func)());
};

////////////////////////////////////////////////////////
// Variable Value Classes
struct CompVarType 
{
	CompVarType();
	virtual ~CompVarType();

	// Will cast memory that void* points to to the approriate type and will set value.
	virtual void
	setValueFromComponent(void* const pVal) = 0;
};

struct BoolVar : public CompVarType 
{
	BoolVar();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const BoolVar &otherVar) const;

	bool value;
};

struct IntVar : public CompVarType 
{
	IntVar();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const IntVar &otherVar) const;

	int32_t value;
};

struct UintVar : public CompVarType 
{
	UintVar();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const UintVar &otherVar) const;

	uint32_t value;
};

struct FloatVar : public CompVarType 
{
	FloatVar();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const FloatVar &otherVar) const;

	float value;
};

struct Float2Var : public CompVarType 
{
	Float2Var();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const Float2Var &otherVar) const;

	float2 value;
};

struct Float3Var : public CompVarType 
{
	Float3Var();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const Float3Var &otherVar) const;

	float3 value;
};

struct Float4Var : public CompVarType 
{
	Float4Var();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const Float4Var &otherVar) const;

	float4 value;
};

struct TransformMatrixVar : public CompVarType 
{
	TransformMatrixVar();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const TransformMatrixVar &otherVar) const;

	mat4 value;
};

struct StringVar : public CompVarType 
{
	StringVar();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const StringVar &otherVar) const;

	StringVar& 
	operator= (const StringVar& other);

	StringVar(const StringVar& other);

	char value[MAX_COMPONENT_STRING_SIZE];
};

struct GeometryVar : public StringVar
{
	GeometryVar();
};

struct EnumVar : public CompVarType 
{
	EnumVar();

	void
	setValueFromComponent(void* const pVal);

	bool 
	operator ==(const EnumVar &otherVar) const;

	int32_t value;	
};
////////////////////////////////////////////////////////

// Base component representation class
// Use Macros to generate derived classes!
// This is just a skeleton for the derived classes
class ComponentRepresentation
{
public:
	// Constructor will pass in the component object by ref.
	// *ComponentClassRepresentation*( *ComponentType* * const component) { Init(component); }
	virtual ~ComponentRepresentation(){}
	
	virtual eastl::unordered_map<eastl::string, ComponentVariableId> const& getNamedVariableIds() const = 0;

	virtual eastl::unordered_map<ComponentVariableId, ComponentVarRepresentation> const&
	getVariableRepresentations() const = 0; // will return *ComponentClass*_varRepresentations

	virtual eastl::vector<ComponentVariableId> const&
	getOrderedVariableIds() const = 0; // will return *ComponentClass*_orderedVarIds

	// T must derive CompVarType and MUST match the varId's type... otherwise things will be really bad.
	template <class T> void 
	getVariableValue(ComponentVariableId const varId, T& varValueOut) const;

	// this will modify the component that was passed in the constructor
	// T must derive CompVarType and MUST match the varId's type... otherwise things will be really bad.
	template <class T, typename InternalType> void 
	setVariableValue(ComponentVariableId const varId, T const& varValueOut);	

	template <class T, typename InternalType>
	void setStringVariableValue(ComponentVariableId const varId, T const& varValueOut);

	virtual uint32_t getComponentID() const = 0;
	virtual const char* getComponentName() const = 0;

	const eastl::unordered_map<eastl::string, void*>& getMetadata(ComponentVariableId const varId)
	{
		return m_ComponentMetadata[varId];
	}

protected:
	// Builds m_ComponentVarReferences & prepares var representations
	// void
	// Init(*ComponentType* * const component);

	eastl::unordered_map<ComponentVariableId, void*> m_ComponentVarReferences;
	eastl::unordered_map<ComponentVariableId, eastl::unordered_map<eastl::string, void*> > m_ComponentMetadata;

	// static const eastl::unordered_map<ComponentVariableId, ComponentVarRepresentation>	*ComponentClass*_varRepresentations;	
};

template <class T, typename InternalType>
void ComponentRepresentation::setStringVariableValue(ComponentVariableId const varId, T const& varValueOut)
{
	eastl::unordered_map<ComponentVariableId, void*>::iterator iter = m_ComponentVarReferences.find(varId);
	if (iter == m_ComponentVarReferences.end())
		return;

	InternalType val = varValueOut.value;
	char* pDest= static_cast<char*>(iter->second);
	strcpy_s(pDest, MAX_COMPONENT_STRING_SIZE, val.c_str());
}

template <class T, typename InternalType>
void ComponentRepresentation::setVariableValue(ComponentVariableId const varId, T const& varValueOut)
{
	eastl::unordered_map<ComponentVariableId, void*>::iterator iter = m_ComponentVarReferences.find(varId);
	if (iter == m_ComponentVarReferences.end())
		return;

	InternalType val = varValueOut.value;
	InternalType* pDest = static_cast<InternalType*>(iter->second);
	*pDest = val;
}

template <class T>
void ComponentRepresentation::getVariableValue(ComponentVariableId const varId, T& varValueOut) const
{
	eastl::unordered_map<ComponentVariableId, void*>::const_iterator iter = m_ComponentVarReferences.find(varId);
	ASSERT(iter != m_ComponentVarReferences.end()); // var not found!

	varValueOut.setValueFromComponent(iter->second);	
}


// Example of how the event metadata will be structures
/*
template <class T> // must derive CompVarType
struct EventModComponent 
{	
	entityid entId;
	componentid compId;
	varid varid;
	T NewValue;
};
Create an object like so: EventModComponent<ColorVar> metadata;
*/

struct VarRepresentationsAndOrderedIds
{
	eastl::unordered_map<ComponentVariableId, ComponentVarRepresentation> representation;
	eastl::vector<ComponentVariableId> orderedIds;
	eastl::unordered_map<eastl::string, ComponentVariableId> varNames;
};

////////////////////////////////////////////////////////
// Helper Struct
// DO NOT USE DIRECTLY!! USE MACROS INSTEAD!!
// These functions should NEVER EVER be called directly.
struct ComponentRepresentationBuilder
{
	// Add var representation and a unique id
	void addVariableRepresentation(eastl::string const& lookup, ComponentVariableId const id, ComponentVarRepresentation const& repr);
	VarRepresentationsAndOrderedIds	create();

private:
	VarRepresentationsAndOrderedIds m_Tmp;

};
////////////////////////////////////////////////////////

}


/////// FOLLOWING ARE MACROS ///////

///////// In .h ////////////////////////////////////////////////////////////////////////////////////////
#define FORGE_START_GENERATE_COMPONENT_REPRESENTATION( componentClass ) \
class componentClass##Representation : public FCR::ComponentRepresentation \
{ \
public:\
	componentClass##Representation() {} \
	\
	componentClass##Representation(componentClass* const component) \
	{ \
		associateComponent(component); \
	} \
	\
	eastl::unordered_map<ComponentVariableId, FCR::ComponentVarRepresentation> const& \
	getVariableRepresentations() const override {return componentClass##VarRepresentationsAndOrderedIds.representation;} \
	\
	virtual eastl::vector<ComponentVariableId> const& \
	getOrderedVariableIds() const override {return componentClass##VarRepresentationsAndOrderedIds.orderedIds;} \
	virtual eastl::unordered_map<eastl::string, ComponentVariableId> const&\
	getNamedVariableIds() const override\
	{return componentClass##VarRepresentationsAndOrderedIds.varNames;}\
	\
	static uint32_t ComponentID; \
	\
	virtual uint32_t getComponentID() const override { return ComponentID; }\
	virtual const char* getComponentName() const override { return #componentClass; }\
	\
	void associateComponent(componentClass* const component); \
	\
	static void BUILD_VAR_REPRESENTATIONS(); \
	static void DESTROY_VAR_REPRESENTATIONS() \
	{ \
		componentClass##VarRepresentationsAndOrderedIds.representation.clear(true); \
		componentClass##VarRepresentationsAndOrderedIds.orderedIds.set_capacity(0); \
		componentClass##VarRepresentationsAndOrderedIds.varNames.clear(true); \
	} \
private: \
	static FCR::VarRepresentationsAndOrderedIds componentClass##VarRepresentationsAndOrderedIds; \
public:

#define FORGE_START_GENERATE_COMPONENT_REPRESENTATION_WITH_RESOURCE_DATA( componentClass, resourceClass ) \
class componentClass##Representation : public FCR::ComponentRepresentation \
{ \
public:\
	componentClass##Representation() {} \
	\
	componentClass##Representation(componentClass* const component) \
	{ \
		associateComponent(component); \
	} \
	componentClass##Representation(componentClass* const component, resourceClass* const resource) \
	{ \
		associateComponent(component); \
		associateResource(resource); \
	} \
	\
	eastl::unordered_map<ComponentVariableId, FCR::ComponentVarRepresentation> const& \
	getVariableRepresentations() const override {return componentClass##VarRepresentationsAndOrderedIds.representation;} \
	\
	virtual eastl::vector<ComponentVariableId> const& \
	getOrderedVariableIds() const override {return componentClass##VarRepresentationsAndOrderedIds.orderedIds;} \
virtual eastl::unordered_map<eastl::string, ComponentVariableId> const& getNamedVariableIds() const override\
	{return componentClass##VarRepresentationsAndOrderedIds.varNames;}\
	\
	static const uint32_t ComponentID; \
	\
	virtual uint32_t getComponentID() const override { return ComponentID; }\
	virtual const char* getComponentName() const override { return #componentClass; }\
	\
	void associateComponent(componentClass* const component); \
	\
	void associateResource(resourceClass* const resource); \
	\
	private: \
	static const FCR::VarRepresentationsAndOrderedIds componentClass##VarRepresentationsAndOrderedIds; \
public:

// Use this to register all the variables you want to expose from the component
#define FORGE_REGISTER_COMPONENT_VAR(varName) \
	static const ComponentVariableId varName;

#define FORGE_END_GENERATE_COMPONENT_REPRESENTATION }; 


///////// In .cpp //////////////////////////////////////////////////////////////////////////////////////
#define FORGE_DEFINE_COMPONENT_ID(componentClass) uint32_t componentClass##Representation::ComponentID;
#define FORGE_INIT_COMPONENT_ID( componentClass ) \
	componentClass##Representation::ComponentID = FCR::UniqueIdGenerator::generateUniqueId(#componentClass, componentClass::GenerateComponent);

// This macro matches REGISTER_COMPONENT_VAR macro
#define FORGE_ASSIGN_UNIQUE_ID_TO_REGISTERED_COMPONENT(componentClass, varName, varid) \
	const ComponentVariableId componentClass##Representation::varName = varid;
//FCR::UniqueIdGenerator::GenerateUniqueId();

#define FORGE_START_VAR_REPRESENTATIONS_BUILD( componentClass ) \
	void componentClass##Representation::BUILD_VAR_REPRESENTATIONS() \
	{ \
	FCR::ComponentRepresentationBuilder builder;

// varName should match a registered var name
#define FORGE_CREATE_VAR_REPRESENTATION(componentClass, varName) \
	ComponentVariableId varName##_id = componentClass##Representation::varName; \
	FCR::ComponentVarRepresentation varName;

#define FORGE_ADD_VAR_MIN_MAX_STEP(varName, fMin, fMax, fStep) \
	varName.min = fMin; \
	varName.max = fMax; \
	varName.step = fStep;

#define FORGE_ADD_PER_COMPONENT_FORMAT(varName, formatStr) \
	varName.perComponentFormat = formatStr; \
	varName.hasPerComponentFormat = true;

// CREATE_VAR_REPRESENTATION should already have been used with varName
#define FORGE_ADD_VALUE_REPRESENTATION(varName, value, stringRepr) \
	varName.valueRepresentations[value] = stringRepr;

// CREATE_VAR_REPRESENTATION should already have been used with varName
#define FORGE_FINALIZE_VAR_REPRESENTATION(varName, varStrName, varType, varAccess) \
	varName.name = varStrName; \
	varName.type = varType; \
	varName.access = varAccess; \
	builder.addVariableRepresentation(#varName, varName##_id, varName);

#define FORGE_END_VAR_REPRESENTATIONS_BUILD( componentClass ) \
	componentClass##VarRepresentationsAndOrderedIds = builder.create(); \
} \
FCR::VarRepresentationsAndOrderedIds componentClass##Representation::componentClass##VarRepresentationsAndOrderedIds;

#define FORGE_START_VAR_REFERENCES( componentClass ) \
	void componentClass##Representation::associateComponent(componentClass* const component) \
	{ 

/* EX:	componentClass = CameraComponent
 *		varName = EnableDebugDraw which was registered with REGISTER_COMPONENT_VAR
 *		varAcces = cameraControls.EnableDebugDraw
 */
#define FORGE_ADD_VAR_REF(componentClass, varName, varAccess) \
	m_ComponentVarReferences[componentClass##Representation::varName] = static_cast<void*>(&(component->varAccess));

#define FORGE_ADD_VAR_METADATA(componentClass, varName, metaName, metadata) \
	m_ComponentMetadata[componentClass##Representation::varName].insert({ metaName , metadata });

#define FORGE_END_VAR_REFERENCES	}

#define FORGE_START_RESOURCE_VAR_REFERENCES( componentClass, resourceClass ) \
	void componentClass##Representation::associateResource(resourceClass* const resource) \
			 { 

#define FORGE_ADD_RESOURCE_VAR_REF(componentClass, varName, varAccess) \
	m_ComponentVarReferences[componentClass##Representation::varName] = static_cast<void*>(&(resource->varAccess)); \
	FORGE_ADD_VAR_METADATA(componentClass, varName, "noserialize", NULL)

 #define FORGE_END_RESOURCE_VAR_REFERENCES	}

