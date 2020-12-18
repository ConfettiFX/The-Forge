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