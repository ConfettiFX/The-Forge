tinystl
=======
Tiny (as in minimal) implementation of some core STL functionality

Contact
-------
[@MatthewEndsley](https://twitter.com/#!/MatthewEndsley)  
<https://github.com/mendsley/tinystl>

License
-------
Copyright 2012-2015 Matthew Endsley

This project is governed by the BSD 2-clause license. For details see the file
titled LICENSE in the project root folder.

Compiling
---------
tinystl is a header only library. But there's some tests that need to be compiled if you want to run them.

1. Get the premake4 binary here: <http://sourceforge.net/projects/premake/files/Premake/4.4/premake-4.4-beta4-windows.zip/download>
2. It's one file, put it somewhere useful! (maybe we'll include it in tinystl later on)
3. Update git submodules: $ git submodule update --init
4. Generate project files
		premake4 vs2008
5. Open your project file. It's in TinySTL/.build/projects/
6. Enjoy a tasty beverage