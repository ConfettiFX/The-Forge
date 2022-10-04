[![logo](media/icon/ozz-grey-256.png)](http://guillaumeblanc.github.io/ozz-animation/)

ozz-animation
=============

open source c++ 3d skeletal animation library and toolset
---------------------------------------------------------

ozz-animation provides runtime character animation playback functionalities (loading, sampling, blending...). It proposes a low-level renderer agnostic and game-engine agnostic implementation, focusing on performance and memory constraints with a data-oriented design.

ozz-animation comes with the toolchain to convert from major Digital Content Creation formats (Fbx, Collada, Obj, 3ds, dxf) to ozz optimized runtime structures. Offline libraries are also provided to implement the conversion from any other animation and skeleton format.

Documentation
-------------

Documentation and samples are available from [ozz-animation website](http://guillaumeblanc.github.io/ozz-animation/).

Supported platforms
-------------------

Ozz is tested on Linux, Mac OS and Windows. The run-time code (ozz_base, ozz_animation, ozz_geometry) depends only on c++03, the standard CRT and has no OS specific code, portability to any other plateform shouldn't be an issue.

Samples, tools and tests depend on external libraries (glfw, Fbx SDK, jsoncpp, gtest, ...), which could limit portability.

Build status
------------

|         | Linux  | Mac OS | Windows |
| ------- | ------ | ------ | ------- |
| master  | [![Travis-CI](https://travis-ci.org/guillaumeblanc/ozz-animation.svg?branch=master)](http://travis-ci.org/guillaumeblanc/ozz-animation) | [![Travis-CI](https://travis-ci.org/guillaumeblanc/ozz-animation.svg?branch=master)](http://travis-ci.org/guillaumeblanc/ozz-animation) | [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/guillaumeblanc/ozz-animation?branch=master&svg=true)](http://ci.appveyor.com/project/guillaumeblanc/ozz-animation) |
| develop | [![Travis-CI](https://travis-ci.org/guillaumeblanc/ozz-animation.svg?branch=develop)](http://travis-ci.org/guillaumeblanc/ozz-animation) | [![Travis-CI](https://travis-ci.org/guillaumeblanc/ozz-animation.svg?branch=develop)](http://travis-ci.org/guillaumeblanc/ozz-animation) | [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/guillaumeblanc/ozz-animation?branch=develop&svg=true)](http://ci.appveyor.com/project/guillaumeblanc/ozz-animation) |

The dashboard for all branches is available [here](http://guillaumeblanc.github.io/ozz-animation/documentation/dashboard/).

Contributing
------------

Contributions are welcome: code review, bug fix, feature implementation...

ozz branching strategy follows [gitflow model](http://nvie.com/posts/a-successful-git-branching-model/). When submitting patches, please:
  - Make pull requests to develop branch for features, to release branch for hotfixes.
  - Do not include merge commits in pull requests; include only commits with the new relevant code.
  - Add all relevant unit tests.
  - Run all the tests and make sure they pass.

License
-------

ozz-animation is hosted on [github](http://github.com/guillaumeblanc/ozz-animation/) and distributed under the **MIT License** (MIT).