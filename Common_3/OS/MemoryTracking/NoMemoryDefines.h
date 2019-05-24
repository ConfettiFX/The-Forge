
#ifndef THEFORGE_INCLUDE_NOMEMORYDEFINES_H
#define THEFORGE_INCLUDE_NOMEMORYDEFINES_H

#ifdef	malloc
#undef	malloc
#endif

#ifdef	calloc
#undef	calloc
#endif

#ifdef	realloc
#undef	realloc
#endif

#ifdef	free
#undef	free
#endif

#ifdef	new
#undef	new
#endif

#ifdef	delete
#undef	delete
#endif

#endif