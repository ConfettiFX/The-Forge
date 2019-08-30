This version of MCPP is forked from the zeroc ice project:
https://github.com/zeroc-ice/ice

There are no major changes. Just a few minor tweaks to make it run as a library, and pass data back and forth
using buffers as opposed to writing files to disk. Also, many of the default warnings were removed. For example,
mcpp expects that last line in the file to be a single line newline file, but warnings like that are just noise.

The other change is adding #pragmas to disable warnings. Since we don't know how the code works, it was safer
to disable the warning than to actually fix the code.

Finally, keep in mind that these functions are not threadsafe. In the future it might make sense to put
all global state into an organized structure and pass that structure to mcpp_lib_main(), but it has not
been a priority. That being said, it should be a relatively easy modification.


