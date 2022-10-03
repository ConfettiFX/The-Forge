/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#pragma once

#include "../../Application/Config.h"

#include "../ThirdParty/OpenSource/bstrlib/bstrlib.h"
#include "../ThirdParty/OpenSource/Nothings/stb_ds.h"

#ifdef __cplusplus
extern "C" 
{
#if 0
}
#endif
#endif
	
/* void bhfree(T*) */
#define bhfree_impl stbds_hmfree_impl
#define bhfree(t) bhfree_impl(t, __FILE__, __LINE__, __FUNCTION__, "bhfree")
/* ptrdiff_t bhlen(T*) */
#define bhlen(t) stbds_hmlen(t)
/* size_t bhlenu(T*) */
#define bhlenu(t) stbds_hmlenu(t)

/*
 * Note: NULL check ensures that there will be no memory allocation,
 *       otherwise  stbds_hmget_key_wrapper can allocate memory for default block
 */
#define bhgeti_unsafe_impl(t, k, f, l, fn, p) \
	((t) == NULL ? -1 : \
		(stbds_hmget_key_wrapper((t), sizeof *(t), STBDS_ALIGNOF_PTR((t)), (void*) STBDS_ADDRESSOF((t)->key, (k)), sizeof (t)->key, STBDS_HM_CONST_BSTRING STBDS_IF_MEM_TRACKING(,f,l,fn,p)), \
		stbds_temp((t)-1)))
#define bhgeti_unsafe(t, k) bhgeti_unsafe_impl(t, k, __FILE__, __LINE__, __FUNCTION__, "bhgeti_unsafe")

#define bhgeti_impl(t, k, f, l, fn, p) \
	((t) = stbds_hmget_key_wrapper((t), sizeof *(t), STBDS_ALIGNOF_PTR((t)), (void*) STBDS_ADDRESSOF((t)->key, (k)), sizeof (t)->key, STBDS_HM_CONST_BSTRING STBDS_IF_MEM_TRACKING(,f,l,fn,p)), \
	 stbds_temp((t)-1))
#define bhgeti(t, k) bhgeti_impl(t, k, __FILE__, __LINE__, __FUNCTION__, "bhgeti")

/* get value */
#define bhget(t, k) (bhgetp_impl(t, k, __FILE__, __LINE__, __FUNCTION__, "bhget")->value)
#define bhgets(t, k) (*bhgetp_impl(t, k, __FILE__, __LINE__, __FUNCTION__, "bhgets"))

#define bhgetp_impl(t, k, f, l, fn, p) \
    ((void) bhgeti_impl(t,k,f,l,fn,p), &(t)[stbds_temp((t)-1)])
#define bhgetp(t, k)  bhgetp_impl(t, k,  __FILE__, __LINE__, __FUNCTION__, "bhgetp")


#define bhgetp_null(t,k) \
	(bhgeti_unsafe_impl(t, k, __FILE__, __LINE__, __FUNCTION__, "bhgetp_null") == -1 ? NULL : &(t)[stbds_temp((t)-1)])

#define bhdefault(t, v) stbds_hmdefault_impl(t, v, __FILE__, __LINE__, __FUNCTION__, "bhdefault")

#define bhdefaults(t, s) stbds_hmdefaults_impl(t, s, __FILE__, __LINE__, __FUNCTION__, "bhdefaults")

#define bhput_impl(t, k, v, f, l, fn, p) \
		((t) = stbds_hmput_key_wrapper((t), sizeof *(t), STBDS_ALIGNOF_PTR((t)), (void*) STBDS_ADDRESSOF((t)->key, (k)), sizeof (t)->key, STBDS_HM_CONST_BSTRING STBDS_IF_MEM_TRACKING(,f,l,fn,p)),   \
		 (t)[stbds_temp((t)-1)].key = (k),    \
		 (t)[stbds_temp((t)-1)].value = (v))
#define bhput(t, k, v) bhput_impl(t, k, v, __FILE__, __LINE__, __FUNCTION__, "bhput")

#define bhputs_impl(t, s, f, l, fn, p) \
		((t) = stbds_hmput_key_wrapper((t), sizeof *(t), STBDS_ALIGNOF_PTR((t)), &(s).key, sizeof (s).key, STBDS_HM_CONST_BSTRING STBDS_IF_MEM_TRACKING(,f,l,fn,p)), \
		 (t)[stbds_temp((t)-1)] = (s))
#define bhputs(t, s) bhputs_impl(t, s, __FILE__, __LINE__, __FUNCTION__, "bhputs")

#define bhdel_impl(t, k, f, l, fn, p) \
		(((t) = stbds_hmdel_key_wrapper((t), sizeof *(t), (void*) STBDS_ADDRESSOF((t)->key, (k)), sizeof (t)->key, STBDS_OFFSETOF((t),key), STBDS_HM_CONST_BSTRING STBDS_IF_MEM_TRACKING(,f,l,fn,p))), \
		 (t)?stbds_temp((t)-1):0)
#define bhdel(t, k) bhdel_impl(t, k, __FILE__, __LINE__, __FUNCTION__, "bhdel")

#ifdef STBDS_UNIT_TESTS
static void stbds_bstring_unit_tests()
{
	struct { bstring key;        int value; }  *strmap = NULL, s;

	{
		s.key = bconstfromliteral("a");
		s.value = 1;
		bhputs(strmap, s);
		ASSERT(strmap != NULL );
		ASSERT(*strmap[0].key.data == 'a'); //-V595
		ASSERT(strmap[0].key.data == s.key.data);
		ASSERT(strmap[0].value == s.value);
		bhfree(strmap);
	}

	/* bstrings can't be managed by hash map */
	//{
	//	s.key = bconstfromliteral("a");
	//	s.value = 1;
	//	sh_new_strdup(strmap);
	//	shputs(strmap, s);
	//	ASSERT(*strmap[0].key == 'a');
	//	ASSERT(strmap[0].key != s.key);
	//	ASSERT(strmap[0].value == s.value);
	//	shfree(strmap);
	//}

	/* bstrings can't be managed by hash map */
	//{
	//	s.key = "a", s.value = 1;
	//	sh_new_arena(strmap);
	//	shputs(strmap, s);
	//	ASSERT(*strmap[0].key == 'a');
	//	ASSERT(strmap[0].key != s.key);
	//	ASSERT(strmap[0].value == s.value);
	//	shfree(strmap);
	//}

	//for (int j = 0; j < 2; ++j) {
	//	ASSERT(shgeti(strmap, "foo") == -1);

	//	if (j == 0)
	//		sh_new_strdup(strmap);
	//	else
	//		sh_new_arena(strmap);
	//	ASSERT(shgeti(strmap, "foo") == -1);
	//	shdefault(strmap, -2);
	//	ASSERT(shgeti(strmap, "foo") == -1);
	//	for (i = 0; i < testsize; i += 2)
	//		shput(strmap, strkey(i), i * 3);
	//	for (i = 0; i < testsize; i += 1)
	//		if (i & 1) { ASSERT(shget(strmap, strkey(i)) == -2); }
	//		else { ASSERT(shget(strmap, strkey(i)) == i * 3); }
	//	for (i = 2; i < testsize; i += 4)
	//		shdel(strmap, strkey(i)); // delete half the entries
	//	for (i = 0; i < testsize; i += 1)
	//		if (i & 3) { ASSERT(shget(strmap, strkey(i)) == -2); }
	//		else { ASSERT(shget(strmap, strkey(i)) == i * 3); }
	//	for (i = 0; i < testsize; i += 1)
	//		shdel(strmap, strkey(i)); // delete the rest of the entries
	//	for (i = 0; i < testsize; i += 1)
	//	{
	//		ASSERT(shget(strmap, strkey(i)) == -2);
	//	}
	//	shfree(strmap);
	//}

	{
		struct { bstring key; char value; } *hash = NULL;
		bstring name = bconstfromliteral("jen");
		bstring bob = bconstfromliteral("bob");
		bhput(hash, bob, 'h');
		ASSERT(bhget(hash, bob) == 'h');
		bhput(hash, bob, 'e');
		ASSERT(bhget(hash, bob) == 'e');
		bhput(hash, bob, 'l');
		ASSERT(bhget(hash, bob) == 'l');
		bhput(hash, bob, 'x');
		ASSERT(bhget(hash, bob) == 'x');
		bhput(hash, bob, 'o');
		ASSERT(bhget(hash, bob) == 'o');

		bhput(hash, name, 'l');
		ASSERT(bhget(hash, name) == 'l');
		bhfree(hash);
	}
}

#endif // STBDS_UNIT_TESTS


#ifdef __cplusplus
}
#endif
