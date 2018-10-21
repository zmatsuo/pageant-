cat <<EOF > version.cpp
#include "version.h"

const char ver[] =
	"`git log -1 --date=format:%y-%m-%d --pretty=format:"%ad"`"
	;
const char commitid[] = "`git rev-parse HEAD`";
EOF

cat <<EOF > version.rc
/* version.rc */

#define VER_FILEVERSION             0,0,999,0
#define VER_FILEVERSION_STR         "0.0.0.999\0"

#define VER_PRODUCTVERSION          0,0,0,999
#define VER_PRODUCTVERSION_STR      "0.0.0.999\0"

/* Version-information resource identifier.  */
#define VS_VERSION_INFO 1

VS_VERSION_INFO VERSIONINFO
FILEVERSION     VER_FILEVERSION
PRODUCTVERSION  VER_PRODUCTVERSION
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904E4"
        BEGIN
            VALUE "ProductName",      "pageant+"
            VALUE "FileDescription",  "pageant+ based on pageant in putty project"
            VALUE "FileVersion",      VER_FILEVERSION_STR
            VALUE "ProductVersion",   VER_PRODUCTVERSION_STR
            VALUE "CompanyName",      "private"
            VALUE "LegalCopyright",   "Copyright (C) 2018 Masaaki Matsuo MIT license"
        END
    END

    BLOCK "VarFileInfo"
    BEGIN
        /* The following line should only be modified for localized versions.     */
        /* It consists of any number of WORD,WORD pairs, with each pair           */
        /* describing a language,codepage combination supported by the file.      */
        /*                                                                        */
        /* For example, a file might have values "0x409,1252" indicating that it  */
        /* supports English language (0x409) in the Windows ANSI codepage (1252). */

        VALUE "Translation", 0x409, 1252
    END
END


// Local Variables:
// coding: utf-8
// End:
EOF
