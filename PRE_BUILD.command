#!/bin/bash

# Change active working directory in case we run script for outside of TheForge
cd "$(dirname "$0")"

filename=Art.zip

rm $filename

curl -L -o $filename http://www.conffx.com/$filename
unzip $filename

rm $filename

exit 0