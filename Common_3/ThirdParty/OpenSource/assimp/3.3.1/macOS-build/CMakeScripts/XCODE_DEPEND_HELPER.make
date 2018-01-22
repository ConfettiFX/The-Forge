# DO NOT EDIT
# This makefile makes sure all linkable targets are
# up-to-date with anything they link to
default:
	echo "Do not invoke directly"

# Rules to remove targets that are older than anything to which they
# link.  This forces Xcode to relink the targets from scratch.  It
# does not seem to check these dependencies itself.
PostBuild.zlibstatic.Debug:
/Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/contrib/zlib/Debug/libzlibstatic.a:
	/bin/rm -f /Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/contrib/zlib/Debug/libzlibstatic.a


PostBuild.assimp.Debug:
/Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/code/Debug/libassimp.a:
	/bin/rm -f /Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/code/Debug/libassimp.a


PostBuild.zlibstatic.Release:
/Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/contrib/zlib/Release/libzlibstatic.a:
	/bin/rm -f /Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/contrib/zlib/Release/libzlibstatic.a


PostBuild.assimp.Release:
/Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/code/Release/libassimp.a:
	/bin/rm -f /Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/code/Release/libassimp.a


PostBuild.zlibstatic.MinSizeRel:
/Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/contrib/zlib/MinSizeRel/libzlibstatic.a:
	/bin/rm -f /Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/contrib/zlib/MinSizeRel/libzlibstatic.a


PostBuild.assimp.MinSizeRel:
/Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/code/MinSizeRel/libassimp.a:
	/bin/rm -f /Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/code/MinSizeRel/libassimp.a


PostBuild.zlibstatic.RelWithDebInfo:
/Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/contrib/zlib/RelWithDebInfo/libzlibstatic.a:
	/bin/rm -f /Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/contrib/zlib/RelWithDebInfo/libzlibstatic.a


PostBuild.assimp.RelWithDebInfo:
/Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/code/RelWithDebInfo/libassimp.a:
	/bin/rm -f /Users/jesusgumbau/Confetti/testbed/Common_3/ThirdParty/OpenSource/assimp/3.3.1/macOS-build/code/RelWithDebInfo/libassimp.a




# For each target create a dummy ruleso the target does not have to exist
