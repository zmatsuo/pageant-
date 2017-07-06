#!/usr/bin/perl

use utf8;
binmode STDIN, ":encoding(utf16le):crlf";
binmode STDOUT, "encoding(utf16le):crlf";

# utf16le bom
print "\x{feff}";

print <<EOF;
[Generic]
UseIniFile=1
EOF

while (<>) {
    /^\[HKEY_CURRENT_USER\\Software\\SimonTatham\\PuTTY\\Sessions\\([^\x00-\x20\\*?\%\x7f-\xff.][^\x00-\x20\\*?\%\x7f-\xff]*)\]\r?$/ or 
    /^\[HKEY_CURRENT_USER\\Software\\SimonTatham\\PuTTY\\SshHostKeys\]\r?$/ or next;
    if (defined $1) {
        print <<EOF;

[Session:$1]
EOF
    }else{
        print <<EOF;

[SshHostKeys]
EOF
    }
    while (<>) {
        /^\r?$/ and last;
        /^"([^\"\\]*(?:\\[\"\\][^\"\\]*)*)"=/ or die "Sytax Error!\n$ARGV($.):$_";
        my $name = $1;
        my $value =$'; #'
        $name =~ s/\\([\\"])/$1/g; #"
        if ($value =~ /^dword:([0-9a-f]{8})\r?$/) {
            $value = hex($1);
        }elsif ($value =~ /^"(.*)"\r?$/) {
            $value = $1;
            $value =~ s/\\([\\"])/$1/g; #"
            $value = '"' . $value . '"';
        }else{
            die "Sytax Error!\n$ARGV($.):$_";
        }
        print <<EOF;
$name=$value
EOF
    }
}
	
