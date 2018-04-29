cat <<EOF > version.cpp
#include "version.h"

const char ver[] = "1.0alpha `git describe --tags`";
const char commitid[] = "`git rev-parse HEAD`";
EOF
