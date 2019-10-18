#pragma once

#include "../../Common_3/OS/Interfaces/ILog.h"
#include "../../Common_3/OS/Interfaces/IThread.h"

#include "../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/unordered_set.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/unordered_map.h"
#include "../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

namespace FCR
{
	class ComponentRepresentation;
};

//class BaseComponent;
#include "BaseComponent.h"

// An entity is collection of components.
// An entity has a name.
class Entity
{
	friend class EntityManager; // only entity manager should directly modify entities

public:

	typedef eastl::unordered_map<uint32_t, BaseComponent*>				  ComponentMap;
	typedef eastl::unordered_map<uint32_t, FCR::ComponentRepresentation*> ComponentRepMap;

	Entity()
	{
	}

	~Entity();

	Entity*
	clone() const;

	// Template getter that retrieves a component based on the component type passed in.
	// The passed in pointer will point to the appropriate component if it is found.

	template<typename T> T* getComponent();
	template<typename T> T const* getComponent() const;

	template<typename T> void getComponent(T*& componentOut);

	ComponentMap& getComponents() { return mComponents; }

	FCR::ComponentRepresentation* const
	getComponentRepresentation(uint32_t const compId);

	//void addComponent(BaseComponent*);
private:
	void addComponent(BaseComponent*);	// made this private. only way to add component is via EntityManager

	ComponentMap	mComponents;
	ComponentRepMap	mComponentRepresentations;
};


template<typename T>
T* Entity::getComponent()
{
	T* componentOut = NULL;

	ComponentMap::iterator it = mComponents.find(T::getTypeStatic());
	if (it != mComponents.end())
		componentOut = (T*)it->second;

	//ASSERT((componentOut != nullptr) && "Couldn't find desired component on entity.");
	
	return componentOut;
}

template<typename T>
T const* Entity::getComponent() const
{
	return const_cast<Entity*>(this)->getComponent<T>();
}

template<typename T>
void Entity::getComponent(T*& componentOut)
{
	componentOut = getComponent<T>();
}

typedef int32_t EntityId;

typedef eastl::unordered_map<EntityId, Entity*>					 EntityMap;
typedef eastl::unordered_map<EntityId, Entity*>::iterator		 EntityMapIterator;
typedef eastl::unordered_map<EntityId, Entity*>::const_iterator  EntityMapConstIterator;
//typedef eastl::hash_map<EntityId, Entity*>					 EntityMapNode;

typedef eastl::hash_map<EntityId, BaseComponent*>				 ComponentLookup;
typedef const ComponentLookup									 Lookup;
typedef eastl::hash_map<uint32_t, ComponentLookup>				 ComponentViseMap;
typedef eastl::pair<EntityId, BaseComponent*>					 Pair;


class EntityManager
{
public:
	EntityManager();
	~EntityManager();

	EntityId createEntity();
	EntityId cloneEntity(EntityId id);
	void deleteEntity(EntityId id);

	Entity* getEntityById(EntityId const id);
    
	bool entityExist(EntityId const id);

	void reset();

	const eastl::unordered_map<EntityId, Entity*>& getEntities() const { return mEntities; }

	template <typename T>
	T& addComponentToEntity(EntityId id);

	template <typename T>
	Lookup& getByComponent()
	{
		Lookup* map = nullptr;

		ComponentViseMap::iterator itr = mComponentViseMap.find(T::getTypeStatic());
		if (itr != mComponentViseMap.end())
		{
			const Lookup* map = &(itr->second);
			return *map;
		}

		return *map;
	}

private:
	Mutex mIdMutex;
	Mutex mEntitiesMutex;
	Mutex mComponentMutex;
	// Entities book-keeping data-structures ////////////////////////
	/* Note:	for now we clump all entities in one data-structure.
	 *			In the future however, as the complexities of the scenes we
	 *			support increases, we may need to be smarter about how we
	 *			store and retrieve entities as to minimize the iterations we
	 *			do over the data-structures.
	 */
	eastl::unordered_map<EntityId, Entity*>			mEntities;
	//eastl::unordered_map<EntityId, EEntityType>		mEntitiesType;
	eastl::unordered_map<eastl::string, EntityId>	mEntitiesName;

	// incr. on entity creation... used to fetch entities.
	EntityId										mEntityIdCounter;
	/////////////////////////////////////////////////////////////////

	ComponentViseMap mComponentViseMap;
};


template <typename T>
T& EntityManager::addComponentToEntity(EntityId _id)
{
	MutexLock lock(mComponentMutex);
	
	BaseComponent* pComponent = nullptr;

	const eastl::unordered_map< uint32_t, ComponentGeneratorFctPtr >& CompGenMap   = ComponentRegistrator::getInstance()->getComponentGeneratorMap();
	eastl::unordered_map< uint32_t, ComponentGeneratorFctPtr >::const_iterator itr = CompGenMap.find(T::getTypeStatic());
	if (itr != CompGenMap.end())
	{
		pComponent = itr->second();
		Entity* pEntity = getEntityById(_id);
		pEntity->addComponent(pComponent);

		ComponentViseMap::iterator itr = mComponentViseMap.find(T::getTypeStatic());
		if (itr != mComponentViseMap.end())
		{
			ComponentLookup& componentMap = itr->second;
			componentMap.insert(Pair(_id, pComponent));
		}
		else
		{
			ASSERT(0);
		}
	}
	else
	{
		ASSERT(0 && "COMPONENT OF GIVEN NAME NOT FOUND");
	}

	return *(static_cast<T*>(pComponent));
}
