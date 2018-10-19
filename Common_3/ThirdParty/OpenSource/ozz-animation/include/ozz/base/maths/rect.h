//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) 2017 Guillaume Blanc                                         //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#ifndef OZZ_OZZ_BASE_MATHS_RECT_H_
#define OZZ_OZZ_BASE_MATHS_RECT_H_

namespace ozz {
namespace math {

// Defines a rectangle by the integer coordinates of its lower-left and
// width-height.
struct RectInt {
  // Constructs a uninitialized rectangle.
  RectInt() {}

  // Constructs a rectangle with the specified arguments.
  RectInt(int _left, int _bottom, int _width, int _height)
      : left(_left), bottom(_bottom), width(_width), height(_height) {}

  // Tests whether _x and _y coordinates are within rectangle bounds.
  bool is_inside(int _x, int _y) const {
    return _x >= left && _x < left + width && _y >= bottom &&
           _y < bottom + height;
  }

  // Gets the rectangle x coordinate of the right rectangle side.
  int right() const { return left + width; }

  // Gets the rectangle y coordinate of the top rectangle side.
  int top() const { return bottom + height; }

  // Specifies the x-coordinate of the lower side.
  int left;
  // Specifies the x-coordinate of the left side.
  int bottom;
  // Specifies the width of the rectangle.
  int width;
  // Specifies the height of the rectangle..
  int height;
};

// Defines a rectangle by the floating point coordinates of its lower-left
// and width-height.
struct RectFloat {
  // Constructs a uninitialized rectangle.
  RectFloat() {}

  // Constructs a rectangle with the specified arguments.
  RectFloat(float _left, float _bottom, float _width, float _height)
      : left(_left), bottom(_bottom), width(_width), height(_height) {}

  // Tests whether _x and _y coordinates are within rectangle bounds
  bool is_inside(float _x, float _y) const {
    return _x >= left && _x < left + width && _y >= bottom &&
           _y < bottom + height;
  }

  // Gets the rectangle x coordinate of the right rectangle side.
  float right() const { return left + width; }

  // Gets the rectangle y coordinate of the top rectangle side.
  float top() const { return bottom + height; }

  // Specifies the x-coordinate of the lower side.
  float left;
  // Specifies the x-coordinate of the left side.
  float bottom;
  // Specifies the width of the rectangle.
  float width;
  // Specifies the height of the rectangle.
  float height;
};
}  // namespace math
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_MATHS_RECT_H_
