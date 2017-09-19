cat <<EOF > version.c
#include "version.h"

const char ver[] = "1.0alpha `git describe`";
const char commitid[] = "`git rev-parse HEAD`";
EOF
