
#ifndef GAINPUTMAPFILTERS_H_
#define GAINPUTMAPFILTERS_H_

namespace gainput
{

/// Inverts the given input value in the range [-1.0f, 1.0f].
GAINPUT_LIBEXPORT float InvertSymmetricInput(float const value, void*);

/// Inverts the given input value in the range [0.0f, 1.0f].
GAINPUT_LIBEXPORT float InvertInput(float const value, void*);

}

#endif

