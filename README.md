[![Build Status](https://travis-ci.com/tbcmangos/core.svg?branch=master)](https://travis-ci.com/tbcmangos/core)

######################
#   HellGroundCore   #
######################

HUGE thanks to the Hellground.net team for temporarily going open-source with their project in 2014
* Wiki: http://wiki.hellground.net
* Site: https://hellground.net

If the Wiki is ever down, there is a dump of the entire website zipped into the repository.

# Why use HellGroundCore?
The common cores like MaNGOS and TrinityCore were abandoned in 2009, in favor of developing 3.x emulation.
They were later taken on again by different forks like OregonCore and SkyfireOne. Even MaNGOS eventually resumed development.

However, they mostly received some update to their libraries and not many of the quality code base was taken and backported to those forks.
As a result, the public emulators are in a fairly bad state of playability. 

HellGroundCore was developed by the team behind the server for players expecting no gamebreaking bugs. It is fairly stable and a great base - only if you're starting development from scratch.

That's 5 years of development other servers don't have. Yet you shouldn't expect something mind blowing. A lot of the structures are very similar to MaNGOS, as it is based on TrinityCore 2009.
It comes with some custom systems as well as improved debug handling/logging and GM permissions and other smaller features like Recruit A Friend.

# Downsides
* Old libraries, especially since this fork hasn't been updated for 2 years
* 64 bit compile under Visual Studio currently doesn't work out of the box
* Not necessarily compatible with fixes ported to cMaNGOS-TBC, OregonCore or TrinityCore2 due to being in closed development for so long


#Compile in Windows
If you intend on compiling it in anything but 32 bit, you will need to use CMake as suggested [here](http://wiki.hellground.net/index.php/Building_under_Windows).

### Disclaimer:
If you prefer compiling your own dependencies, they can be fund under dep/include/*. These have the correct versions, so that you don't have to worry about conflicts with precompiled binaries you may be using!

You can find OpenSSL as an installer for Win32/64 [here](https://slproweb.com/products/Win32OpenSSL.html)
You will also need to make your version of Visual Studio [compatible with the 2010 64 Bit Compiler](http://stackoverflow.com/questions/1865069/how-to-compile-a-64-bit-application-using-visual-c-2010-express)

CMake expects ACE, MySQL and TBB (if you want to use the latter) to be under dep/lib/*, if you aren't using external libraries. 
There are already precompiled libraries you can find under dep/lib/precompiled/Release.

Either edit CMakeLists to your liking or provide MySQL or put your MySQL libraries under dep/lib/[x64_release|win32_release]/*.
