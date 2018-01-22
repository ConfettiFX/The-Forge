filename=Art.zip

rm $filename

curl -L -o $filename http://www.conffx.com/$filename
unzip $filename

rm $filename
