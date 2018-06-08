# DO NOT EDIT
# This makefile makes sure all linkable targets are
# up-to-date with anything they link to
default:
	echo "Do not invoke directly"

# Rules to remove targets that are older than anything to which they
# link.  This forces Xcode to relink the targets from scratch.  It
# does not seem to check these dependencies itself.
PostBuild.zlib.Debug:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/Debug${EFFECTIVE_PLATFORM_NAME}/libzlib.dylib:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/Debug${EFFECTIVE_PLATFORM_NAME}/libzlib.dylib


PostBuild.zlibstatic.Debug:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/Debug${EFFECTIVE_PLATFORM_NAME}/libzlibstatic.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/Debug${EFFECTIVE_PLATFORM_NAME}/libzlibstatic.a


PostBuild.IrrXML.Debug:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/irrXML/Debug${EFFECTIVE_PLATFORM_NAME}/libIrrXML.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/irrXML/Debug${EFFECTIVE_PLATFORM_NAME}/libIrrXML.a


PostBuild.assimp.Debug:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/code/Debug${EFFECTIVE_PLATFORM_NAME}/libassimp.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/code/Debug${EFFECTIVE_PLATFORM_NAME}/libassimp.a


PostBuild.zlib.Release:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/Release${EFFECTIVE_PLATFORM_NAME}/libzlib.dylib:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/Release${EFFECTIVE_PLATFORM_NAME}/libzlib.dylib


PostBuild.zlibstatic.Release:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/Release${EFFECTIVE_PLATFORM_NAME}/libzlibstatic.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/Release${EFFECTIVE_PLATFORM_NAME}/libzlibstatic.a


PostBuild.IrrXML.Release:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/irrXML/Release${EFFECTIVE_PLATFORM_NAME}/libIrrXML.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/irrXML/Release${EFFECTIVE_PLATFORM_NAME}/libIrrXML.a


PostBuild.assimp.Release:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/code/Release${EFFECTIVE_PLATFORM_NAME}/libassimp.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/code/Release${EFFECTIVE_PLATFORM_NAME}/libassimp.a


PostBuild.zlib.MinSizeRel:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libzlib.dylib:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libzlib.dylib


PostBuild.zlibstatic.MinSizeRel:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libzlibstatic.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libzlibstatic.a


PostBuild.IrrXML.MinSizeRel:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/irrXML/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libIrrXML.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/irrXML/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libIrrXML.a


PostBuild.assimp.MinSizeRel:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/code/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libassimp.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/code/MinSizeRel${EFFECTIVE_PLATFORM_NAME}/libassimp.a


PostBuild.zlib.RelWithDebInfo:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libzlib.dylib:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libzlib.dylib


PostBuild.zlibstatic.RelWithDebInfo:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libzlibstatic.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/zlib/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libzlibstatic.a


PostBuild.IrrXML.RelWithDebInfo:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/irrXML/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libIrrXML.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/contrib/irrXML/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libIrrXML.a


PostBuild.assimp.RelWithDebInfo:
/Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/code/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libassimp.a:
	/bin/rm -f /Users/root1/Documents/Confetti/TheForge/Common_3/ThirdParty/OpenSource/assimp/4.1.0/iOS-build/code/RelWithDebInfo${EFFECTIVE_PLATFORM_NAME}/libassimp.a




# For each target create a dummy ruleso the target does not have to exist
