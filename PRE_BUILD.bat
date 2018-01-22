set filename=Art.zip

del %filename%

"BuildTools/wget" -O %filename% http://www.conffx.com/%filename%
"BuildTools/7z" x %filename% > NUL

del %filename%
