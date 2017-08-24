#!/bin/sh
dest=`date +install_%y%m%d_%H%M%S`
version=1.0alpha
windeployqt=C:/Qt/5.9/msvc2015_64/bin/windeployqt.exe
zip=pageant+_${version}_`date +%y%m%d_%H%M%S`.zip
if [ -e ${dest} ]; then
	rm -rf ${dest}
fi
mkdir -p ${dest}/pageant+
cp Release/pageant+.exe ${dest}/pageant+
${windeployqt} ${dest}/pageant+/pageant+.exe
(cd ${dest} && /bin/find . -type f | xargs -I% zip ../${zip} %)
rm -rf ${dest}
