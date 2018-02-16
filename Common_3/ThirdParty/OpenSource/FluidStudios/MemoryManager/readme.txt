--------------------------------------------------------------------------------

You can find the latest version of this source at:

	HTTP://www.FluidStudios.com/publications.html

!IMPORTANT! Please spend just a couple minutes reading the comments at the top
            of the mmgr.cpp file... it will save you a lot of headaches!

--------------------------------------------------------------------------------

This code originally appeared on www.flipcode.com as an entry to the
"Ask Midnight" column titled "Presenting A Memory Manager":

	http://www.flipcode.com/askmid/archives.shtml

Here's the text that appeared with the release of this code:

--------------------------------------------------------------------------------

This installment of the Ask Midnight column is not in response to a question,
but rather a follow-up to the last question about memory management & tracking.
I’ve spent the past week developing and testing a new industrial-grade memory
manager.

During this effort, I found three bugs in an application I thought to be bug-
free (at least, as far as memory usage goes.) I had this confidence because I
often run with BoundsChecker (as well as the memory tracking tools in the
Microsoft libraries.) Neither of these tools was able to locate these particular
bugs, so I assumed the bugs didn’t actually exist. As it turns out, the memory
manager presented here located them and told me exactly where to go to fix them.
As a result, I now have a little more confidence in the application I was
testing with, and even more confidence in my memory tracker. Here’s what this
memory tracker told me:

* I was never deallocating the frame buffer. Sounds simple but the fact is,
  neither BoundsChecker nor Microsoft’s memory tracking library noticed this
  memory leak.

* I noticed that there was over a MEG of unused RAM in the memory report. By
  attaching a memory report dump to a key, I was able to see a snapshot of what
  memory was in use (every single allocation) and noticed that a particular
  block of allocated RAM contained a large amount of unused RAM within it. Going
  to the line in the source where the allocation took place, I found that I was
  allocating a z-buffer from legacy code and had forgot to remove the allocation
  when I removed the rest of the z-buffer code.

* When the application started up, the window would be bright green prior
  rendering the first frame. This was because the "unused memory" fill pattern
  used by the memory manager translates to bright green. Looking at the source,
  I found that I wasn’t clearing the frame buffer after the allocation.

Is your code bug-free? Take the Memory Manager challenge! I urge you to include
this software in your projects as a test. If your projects have any size to them
at all, or use memory in a multitude of various ways, then this memory tracker
may very likely find bugs in your code that you never knew were there. Just
include this software with your project, include the MMGR.H file in each of your
source files and recompile. After each run, check for any MEM*.LOG files in the
project’s directory. If you really want to test your software, simply edit
MMGR.CPP and uncomment the STRESS_TEST macro. This will slow your allocations
down a bit, but it’s worth it. And finally, spend a couple minutes and read the
comments at the top of the MMGR.CPP file.

If this code helps you find bugs you never knew existed or helps you track down
a tough bug, then please visit this issue of Ask Midnight and leave a
testimonial in the comments. I will periodically check them and may eventually
release an updated version of this software based on your comments.

So go ahead. Take the challenge. I dare you. :) 

- Paul Nettle
midnight@FluidStudios.com

