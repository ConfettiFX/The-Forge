#include "BaseComponent.h"

#include "../../Common_3/OS/Interfaces/IMemory.h"    // Must be the last include in a cpp file

ComponentRegistrator* ComponentRegistrator::instance = NULL;

ComponentRegistrator* ComponentRegistrator::getInstance()
{
	if (!instance)
	{
		instance = tf_new(ComponentRegistrator);
		return instance;
	}
	return instance;
}

void ComponentRegistrator::destroyInstance()
{
	if (ComponentRegistrator::instance)
	{
		tf_delete(instance);
	}
}

void ComponentRegistrator::mapInsertion(uint32_t str, ComponentGeneratorFctPtr ptr)
{
	componentGeneratorMap.insert(eastl::pair<uint32_t, ComponentGeneratorFctPtr>(str, ptr));
}

void BaseComponent::instertIntoComponentGeneratorMap(uint32_t component_name, BaseComponent* (*func)())
{
	ComponentRegistrator::getInstance()->mapInsertion(component_name, func);
}