#!/usr/bin/perl

use utf8;
binmode STDIN, ":encoding(utf16le):crlf";
binmode STDOUT, "encoding(utf16le):crlf";

# utf16le bom
print "\x{feff}";

print <<EOF;
REGEDIT4

[HKEY_CURRENT_USER\\Software\\SimonTatham\\PuTTY]

[HKEY_CURRENT_USER\\Software\\SimonTatham\\PuTTY\\Sessions]
EOF

while (<>) {
    /^\[Session:([^\x00-\x20\\*?\%\x7f-\xff.][^\x00-\x20\\*?\%\x7f-\xff]*)\]\r?$/ or 
    /^\[SshHostKeys\]\r?$/ or next;
    if (defined $1) {
        print <<EOF;

[HKEY_CURRENT_USER\\Software\\SimonTatham\\PuTTY\\Sessions\\$1]
EOF
    }else{
        print <<EOF;

[HKEY_CURRENT_USER\\Software\\SimonTatham\\PuTTY\\SshHostKeys]
EOF
    }
    while (<>) {
        /^\r?$/ and last;
        /^([^=]+)=/ or die "Sytax Error!\n$ARGV($.):$_";
        my $name = $1;
        my $value =$'; #'
        $name =~ s/\\([\\"])/$1/g; #"
        if ($value =~ /^(-?\d+)\r?$/) {
            $value = sprintf('dword:%08x', $1);
        }elsif ($value =~ /^"(.*)"\r?$/) {
            $value = $1;
            $value =~ s/[\\"]/\\$&/g; #"
            $value = '"' . $value . '"';
        }else{
            $value =~ s/^\s+//;
            $value =~ s/\s+$//;
            $value =~ s/[\\"]/\\$&/g; #"
            $value = '"' . $value . '"';
        }
        print <<EOF;
"$name"=$value
EOF
    }
}
