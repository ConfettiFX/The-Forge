#include "../../Common_3/OS/Interfaces/ILog.h"

#include "EntityManager.h"
// Components ////////////////////////////////////
//----
//////////////////////////////////////////////////
#include "ComponentRepresentation.h"
// Component Representations -- as to get component ids /////////////////////////////////////////
//----
#include "../../Common_3/OS/Interfaces/IMemory.h" // NOTE: this should be the last include in a .cpp
/////////////////////////////////////////////////////////////////////////////////////////////////

Entity::~Entity()
{
	for (ComponentMap::iterator it = mComponents.begin(); it != mComponents.end(); ++it)
	{
		it->second->~BaseComponent();
		tf_free(it->second);
	}

	for (eastl::pair<uint32_t, FCR::ComponentRepresentation*> repMap_iter : mComponentRepresentations)
	{
		repMap_iter.second->~ComponentRepresentation();
		tf_free(repMap_iter.second);
	}
}

Entity* Entity::clone() const
{
	Entity* new_entity = tf_placement_new<Entity>(tf_calloc(1, sizeof(Entity)));
	
	for (ComponentMap::const_iterator it = mComponents.begin(); it != mComponents.end(); ++it)
	{
		BaseComponent* pNewComponent = NULL;
		pNewComponent = it->second->clone();
		new_entity->addComponent(pNewComponent);
	}
	
	return new_entity;
}

FCR::ComponentRepresentation* const Entity::getComponentRepresentation(uint32_t const compId)
{
	eastl::unordered_map<uint32_t, FCR::ComponentRepresentation*>::iterator iter = mComponentRepresentations.find(compId);

	if (iter == mComponentRepresentations.end())
	{
		ASSERT(0); // No such comp representation found!
	}

	return iter->second;
}


void Entity::addComponent(BaseComponent* component)
{
	if (component)
	{
		uint32_t compType = component->getType();

		ComponentMap::const_iterator const_itr = mComponents.find(compType);
		if (const_itr == mComponents.end())
		{
			FCR::ComponentRepresentation* r = component->createRepresentation();
			mComponents.insert({ compType, component });
			mComponentRepresentations[r->getComponentID()] = r;
		}
		else
		{
			ASSERT(0 && "component for entity already exist");
		}
	}
	else
	{
		ASSERT(0 && "component is null");
	}
}

EntityManager::EntityManager()
{
	mEntityIdCounter = 1; // entity ids will be used in scene graph tree for transformations... 0 will be dedicated to scene root.

	mComponentViseMap.rehash(93);
	const eastl::unordered_map<uint32_t, ComponentGeneratorFctPtr>& CompGenMap = ComponentRegistrator::getInstance()->getComponentGeneratorMap();
	for (eastl::pair<uint32_t, ComponentGeneratorFctPtr> pair : CompGenMap)
	{
		ComponentLookup map;
		map.rehash(11083);
		mComponentViseMap.insert(eastl::pair< uint32_t, ComponentLookup >(pair.first, map));
	}
	
	mEntitiesMutex.Init();
	mIdMutex.Init();
	mComponentMutex.Init();
}

EntityManager::~EntityManager()
{
	reset();
	mEntitiesMutex.Destroy();
	mIdMutex.Destroy();
	mComponentMutex.Destroy();
	ComponentRegistrator::destroyInstance();
}

void EntityManager::reset()
{
	eastl::unordered_map<EntityId, Entity*> entities = getEntities();
	// Release memory for each entity
	for (eastl::pair<EntityId, Entity*> entity : entities)
	{
		deleteEntity(entity.first);
	}
	mEntities.clear();

	// Clear stale component pointers
	for (eastl::pair<uint32_t, ComponentLookup> pair : mComponentViseMap)
	{
		pair.second.clear();
	}
}

EntityId EntityManager::createEntity()
{
	Entity* new_entity = tf_placement_new<Entity>(tf_calloc(1, sizeof(Entity)));

	EntityId id = 0;
	{
		MutexLock lock(mIdMutex);
		id = mEntityIdCounter++;
		MutexLock entLock(mEntitiesMutex);
		mEntities[id] = new_entity;
	}

    // If id < 0, the m_EntityIdCounter is over flow.
    // id == 0 is reserved for SCENE_ROOT.
    ASSERT(id > 0);

	return id;
}

EntityId EntityManager::cloneEntity(EntityId id)
{
	Entity* source_entity = getEntityById(id);
	Entity* new_entity	  = source_entity->clone();

	EntityId newid = 0;
	{
		MutexLock lock(mIdMutex);
		newid = mEntityIdCounter++;
		MutexLock entLock(mEntitiesMutex);
		mEntities[newid] = new_entity;
	}
	
	return newid;
}

void EntityManager::deleteEntity(EntityId id)
{
	ASSERT (id != 0); // 0 is reserved for describing to root of the scene in the scene graph

	// Free components and actual entity memory
	Entity* entity = NULL;

	entity = getEntityById(id);
	ASSERT(entity);

	{
		MutexLock lock(mEntitiesMutex);
		// Unpopulate data structures
		eastl::unordered_map<EntityId, Entity*>::iterator entities_iter = mEntities.find(id);
		ASSERT(entities_iter != mEntities.end());
		mEntities.erase(entities_iter);
	}
	entity->~Entity();
	tf_free(entity);
}


Entity* EntityManager::getEntityById(EntityId const id)
{
	ASSERT (id != 0); // 0 is reserved for describing to root of the scene in the scene graph
	Entity* pEntity = NULL;
	{
		MutexLock lock(mEntitiesMutex);
		EntityMap::iterator iter = mEntities.find(id);
		ASSERT(iter != mEntities.end());
		pEntity = iter->second;
	}
	return pEntity;
}


bool EntityManager::entityExist(EntityId const id)
{
	if (id == 0)
	{
		// 0 is root and always exists
		return true;
	}
	{
		MutexLock lock(mEntitiesMutex);
		EntityMap::iterator iter = mEntities.find(id);
		return (iter != mEntities.end());
	}
}
