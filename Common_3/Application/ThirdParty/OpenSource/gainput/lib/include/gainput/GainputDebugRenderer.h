
#ifndef GAINPUTDEBUGRENDERER_H_
#define GAINPUTDEBUGRENDERER_H_

namespace gainput
{

/// Interface for debug rendering of input device states.
/**
 * Coordinates and other measures passed to the interface's functions are in the
 * range of 0.0f to 1.0f.
 *
 * The functions are called as part InputManager::Update().
 */
class GAINPUT_LIBEXPORT DebugRenderer
{
public:
	/// Empty virtual destructor.
	virtual ~DebugRenderer() { }

	/// Called to draw a circle with the given radius.
	virtual void DrawCircle(float x, float y, float radius) = 0;

	/// Called to draw a line between the two given points.
	virtual void DrawLine(float x1, float y1, float x2, float y2) = 0;

	/// Called to draw some text at the given position.
	virtual void DrawText(float x, float y, const char* const text) = 0;
};

}

#endif

