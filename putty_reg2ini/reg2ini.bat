reg export HKCU\Software\SimonTatham\PuTTY putty_export.reg
perl reg2ini.pl < putty_export.reg > putty_export.ini
pause
