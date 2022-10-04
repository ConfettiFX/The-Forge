## About

**Multithreaded task scheduler experiments.**

**Written under the influence by great GDC talk "Parallelizing the Naughty Dog engine using fibers" by Christian Gyrling**

Compiled and worked on : **Clang 3.4, GCC 4.8.2, MSVC 2010/2012/2015/2017, XCODE 6.4**

## Commercial games using Task Scheduler

- Skyforge (PC, PS4, X1)

## Build status

Linux + OS X
![Travis build status](https://api.travis-ci.org/SergeyMakeev/TaskScheduler.svg?branch=master)

Windows
![Appveyor build status](https://ci.appveyor.com/api/projects/status/7o760ylay8mdplo6)



## Useful reading (in random order):

Parallelizing the Naughty Dog engine using fibers by Christian Gyrling

http://www.swedishcoding.com/wp-content/uploads/2015/03/parallelizing_the_naughty_dog_engine_using_fibers.pdf

id Tech 5 Challenges
From Texture Virtualization to Massive Parallelization by J.M.P. van Waveren

http://s09.idav.ucdavis.edu/talks/05-JP_id_Tech_5_Challenges.pdf

Doom3 BFG Source Code Review: Multi-threading by Fabien Sanglard

http://fabiensanglard.net/doom3_bfg/threading.php

How Ubisoft Develops Games for Multicore - Before and After C++11 by Jeff Preshing

http://www.youtube.com/watch?v=X1T3IQ4N-3g

Killzone Shadow Fall: Threading the Entity Update on PS4 by Jorrit Rouwe

http://www.slideshare.net/jrouwe/killzone-shadow-fall-threading-the-entity-update-on-ps4

Killzone Shadow Fall Demo Postmortem by Michal Valient

http://www.guerrilla-games.com/presentations/Valient_Killzone_Shadow_Fall_Demo_Postmortem.pdf

Infamous Second Son : Engine Postmortem by Adrian Bentley

http://adruab.net/wp-images/GDC14_infamous_second_son_engine_postmortem.pdf

Multithreading the Entire Destiny Engine - GDC 2015 by Barry Genova

http://www.gdcvault.com/play/1022164/Multithreading-the-Entire-Destiny (members only)
http://chomikuj.pl/dexio21/GDC+2015/GDC+Vault+-+Multithreading+the+Entire+Destiny+Engine,4690817362.mp4%28video%29


Intel Threading Building Blocks - Scheduling Algorithm

https://www.threadingbuildingblocks.org/docs/help/reference/task_scheduler/scheduling_algorithm.htm

CILK/CILK++ and Reducers

http://www.slideshare.net/yunmingzhang/yunming-zhang-presentations

Task Scheduling Strategies by Dmitry Vyukov

http://www.1024cores.net/home/scalable-architecture/task-scheduling-strategies

Implementing a Work-Stealing Task Scheduler on the ARM11 MPCore

http://www.rtcgroup.com/arm/2007/presentations/211%20-%20Implementing%20a%20Work-Stealing%20Task%20Scheduler.pdf

Lost Planet graphics course for 3D game fan of Nishikawa Zenji

http://game.watch.impress.co.jp/docs/20070131/3dlp.htm

Dragged Kicking and Screaming: Source Multicore by Tom Leonard

http://www.valvesoftware.com/publications/2007/GDC2007_SourceMulticore.pdf

Games: Playing with Threads by Ben Nicholson

http://www2.epcc.ed.ac.uk/downloads/lectures/BenNicholson/BenNicholson.pdf

Work Stealing by Pablo Halpern 

https://github.com/CppCon/CppCon2015/tree/master/Presentations/Work%20Stealing

Enki Task Scheduler by Doug Binks

http://www.enkisoftware.com/devlogpost-20150822-1-Implementing_a_lightweight_task_scheduler.html

http://www.enkisoftware.com/devlogpost-20150905-1-Internals_of_a_lightweight_task_scheduler.html

Molecule Engine blog - Job System 2.0 by Stefan Reinalter

http://blog.molecular-matters.com/2015/08/24/job-system-2-0-lock-free-work-stealing-part-1-basics/

http://blog.molecular-matters.com/2015/09/08/job-system-2-0-lock-free-work-stealing-part-2-a-specialized-allocator/

http://blog.molecular-matters.com/2015/09/25/job-system-2-0-lock-free-work-stealing-part-3-going-lock-free/

Molecule Engine blog - Building a load-balanced task scheduler by Stefan Reinalter

http://blog.molecular-matters.com/2012/04/05/building-a-load-balanced-task-scheduler-part-1-basics/

http://blog.molecular-matters.com/2012/04/12/building-a-load-balanced-task-scheduler-part-2-task-model-relationships/

http://blog.molecular-matters.com/2012/04/25/building-a-load-balanced-task-scheduler-part-3-parent-child-relationships/

http://blog.molecular-matters.com/2012/07/09/building-a-load-balanced-task-scheduler-part-4-false-sharing/

Do-it-yourself Game Task Scheduling by Jerome Muffat-Meridol

https://software.intel.com/en-us/articles/do-it-yourself-game-task-scheduling

Acquire and Release Semantics by Jeff Preshing

http://preshing.com/20120913/acquire-and-release-semantics/

Lockless Programming Considerations for Xbox 360 and Microsoft Windows

https://msdn.microsoft.com/en-us/library/windows/desktop/ee418650(v=vs.85).aspx

C/C++11 mappings to processors by Peter Sewell

https://www.cl.cam.ac.uk/~pes20/cpp/cpp0xmappings.html 

Memory Ordering in Modern Microprocessors, Part I by Paul E. McKenney
http://www.linuxjournal.com/node/8211/print

Memory Ordering in Modern Microprocessors, Part II by Paul E. McKenney
http://www.linuxjournal.com/node/8212/print
