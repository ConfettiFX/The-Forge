#pragma once

#include "../../Common_3/OS/Interfaces/IOperatingSystem.h"

#include "../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"

//#include "../../Common_3/OS/Interfaces/IMemory.h"

#define MAX_COMPONENT_STRING_SIZE 256

/* Components:
 * Components are the pieces of data that make up an "entity".
 * Anything that can be placed in a scene is an entity.
 *
 * Components's should have a "data" struct which only contains raw data that can be dumped to binary.
 * There should be no logic that modifies the data within a component besides simple
 * functions that would "aid" in setting the data of the component.
 *
 * Since components are pure "raw" data, the default constructor should
 * be adequate to later "clone" entities. This should largely/completely
 * eliminate the amount of custom copy constructors or "Copy()" functions we'll
 * have to write throughout the code base.
 */

namespace FCR { class ComponentRepresentation; }

class BaseComponent;

typedef BaseComponent* (*ComponentGeneratorFctPtr)();

class ComponentRegistrator
{// singleton
	friend class BaseComponent;
public:
	ComponentRegistrator() {}
	~ComponentRegistrator() {}
	inline const eastl::unordered_map<uint32_t, ComponentGeneratorFctPtr>& getComponentGeneratorMap() { return componentGeneratorMap; }

	static ComponentRegistrator* getInstance();
	static void destroyInstance();

private:
	void mapInsertion(uint32_t, ComponentGeneratorFctPtr);
	eastl::unordered_map<uint32_t, ComponentGeneratorFctPtr> componentGeneratorMap;
	static ComponentRegistrator* instance;
};

class BaseComponent
{
public:
	virtual ~BaseComponent() {}
	virtual BaseComponent* clone() const = 0;
	virtual uint32_t getType() const = 0;
	virtual FCR::ComponentRepresentation* createRepresentation() = 0;
	virtual void destroyRepresentation(FCR::ComponentRepresentation* pRep) = 0;

	static void instertIntoComponentGeneratorMap(uint32_t component_name, BaseComponent* (*func)());
};

#define FORGE_DECLARE_COMPONENT(Component_) \
	public: \
		virtual Component_* clone() const override; \
		virtual FCR::ComponentRepresentation* createRepresentation() override; \
		virtual void destroyRepresentation(FCR::ComponentRepresentation* pRep) override; \
		virtual uint32_t getType() const override; \
		static uint32_t getTypeStatic(); \
		static BaseComponent* GenerateComponent(); \
		static eastl::hash<eastl::string> Component_##hashedStr; \
		static uint32_t Component_##typeHash; \
	private:

#define FORGE_IMPLEMENT_COMPONENT(Component_) \
	eastl::hash<eastl::string> Component_::Component_##hashedStr; \
	uint32_t Component_::Component_##typeHash = (uint32_t)Component_##hashedStr(eastl::string(#Component_)); \
	Component_* Component_ ::clone() const { return tf_placement_new<Component_>(tf_calloc(1, sizeof(Component_)), *this); } \
	FCR::ComponentRepresentation* Component_::createRepresentation() { return tf_placement_new<Component_##Representation>(tf_calloc(1, sizeof(Component_##Representation)), this); } \
	void Component_::destroyRepresentation(FCR::ComponentRepresentation* pRep) { pRep->~ComponentRepresentation(); tf_free(pRep); } \
	uint32_t Component_::getTypeStatic() {  return Component_##typeHash; } \
	uint32_t Component_::getType() const { return Component_::getTypeStatic(); } \
	BaseComponent* Component_::GenerateComponent() { return tf_new(Component_); }