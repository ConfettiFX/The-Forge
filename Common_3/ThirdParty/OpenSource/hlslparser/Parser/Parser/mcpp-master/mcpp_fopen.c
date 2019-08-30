#ifdef _WIN32
#  include <Windows.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#pragma warning(disable:4267)

FILE* mcpp_fopen(const char* filename, const char* mode)
{
#ifdef _WIN32
    FILE* f = 0;
    if(filename && mode)
    {
        int wfilenameLength = strlen(filename) + 1;
        wchar_t* wfilename = malloc(wfilenameLength * sizeof(wchar_t));
        if(wfilename)
        {
            if(MultiByteToWideChar(CP_UTF8, 0, filename, -1, wfilename, wfilenameLength))
            {
                int wmodeLength = strlen(mode) + 1;
                wchar_t* wmode = malloc(wmodeLength * sizeof(wchar_t));
                if(wmode)
                {
                    if(MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, wmodeLength))
                    {
                        _wfopen_s(&f, wfilename, wmode);
                    }
                    free(wmode);
                }
            }
            free(wfilename);
        }
    }
    return f;
#else
    return fopen(filename, mode);
#endif
}
