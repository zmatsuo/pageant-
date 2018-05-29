cat <<EOF > version.cpp
#include "version.h"

const char ver[] =
	"1.0alpha `git describe --tags`"
	" `git log -1 --date=format:%y-%m-%d --pretty=format:"%ad"`"
//	" `date +%y-%m-%d`"
	;
const char commitid[] = "`git rev-parse HEAD`";
EOF
