ssh %HLSPARSER_TEST_SSH% "mkdir HLSLParserTest"
sleep 1
ssh %HLSPARSER_TEST_SSH% "rm HLSLParserTest/*"
sleep 1
scp Metal/* %HLSPARSER_TEST_SSH%:HLSLParserTest
sleep 1
ssh %HLSPARSER_TEST_SSH% 'for f in `ls HLSLParserTest/*.metal`; do xcrun -sdk macosx  metal -c $f -o "${f}.air"; done' 2>compile_mac.log
