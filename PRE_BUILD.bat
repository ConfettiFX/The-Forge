set filename=Art.zip

del %filename%

"Tools/wget" -O %filename% http://www.conffx.com/%filename%
"Tools/7z" x %filename% > NUL

del %filename%
