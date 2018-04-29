#!/bin/sh
dest=`date +install_%y%m%d_%H%M%S`
version=1.0alpha
windeployqt=C:/Qt/5.10.1/msvc2017_64/bin/windeployqt.exe
if [ ! -e ${windeployqt} ]; then
	echo windeployqt not found
	exit
fi
zip=pageant+_${version}_`date +%y%m%d_%H%M%S`
if [ -e ${dest} ]; then
	rm -rf ${dest}
fi
grep "^#define DEVELOP_VERSION" pageant+.h > /dev/null
if [ "$?" -eq 0 ]; then
	echo DEVELOP_VERSION
	zip=${zip}_dev
fi
mkdir -p ${dest}/pageant+
cp x64/Release/pageant+.exe ${dest}/pageant+
cp x64/Release/pageant+_rdp_client.dll ${dest}/pageant+
${windeployqt} -no-angle --no-opengl-sw --no-translations ${dest}/pageant+/pageant+.exe
(cd ${dest} && /bin/find . -type f | xargs -I% zip ../${zip}.zip %)
rm -rf ${dest}
