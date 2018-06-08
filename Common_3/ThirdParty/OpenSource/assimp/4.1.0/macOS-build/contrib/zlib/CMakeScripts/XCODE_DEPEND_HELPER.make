# DO NOT EDIT
# This makefile makes sure all linkable targets are
# up-to-date with anything they link to
default:
	echo "Do not invoke directly"

# Rules to remove targets that are older than anything to which they
# link.  This forces Xcode to relink the targets from scratch.  It
# does not seem to check these dependencies itself.
PostBuild.zlib.Debug:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/Debug/libzlib.dylib:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/Debug/libzlib.dylib


PostBuild.zlibstatic.Debug:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/Debug/libzlibstatic.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/Debug/libzlibstatic.a


PostBuild.zlib.Release:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/Release/libzlib.dylib:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/Release/libzlib.dylib


PostBuild.zlibstatic.Release:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/Release/libzlibstatic.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/Release/libzlibstatic.a


PostBuild.zlib.MinSizeRel:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/MinSizeRel/libzlib.dylib:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/MinSizeRel/libzlib.dylib


PostBuild.zlibstatic.MinSizeRel:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/MinSizeRel/libzlibstatic.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/MinSizeRel/libzlibstatic.a


PostBuild.zlib.RelWithDebInfo:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/RelWithDebInfo/libzlib.dylib:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/RelWithDebInfo/libzlib.dylib


PostBuild.zlibstatic.RelWithDebInfo:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/RelWithDebInfo/libzlibstatic.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/macOS-build/contrib/zlib/RelWithDebInfo/libzlibstatic.a




# For each target create a dummy ruleso the target does not have to exist
