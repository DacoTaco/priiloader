---------------------------------------------------------------
* Priiloader Installer - An installation utiltiy for priiloader *
---------------------------------------------------------------



==============================
Compiling / Binding Preloader
==============================

In order to use this installer with your application you must first place a "certs.bin"
in the data directory along with "preloader.app". The "preloader.app" file is your
compiled binary of the preloader source that has been bound with nand booting code. You
can obtain this from any systems menu or channel. This code must be placed at the end of
your compiled binary and potentially padded. You will also need to add the correct loading
address in the dol header, in text section 1. this can be done manually or with dolboot.exe (found in priiloader source folder)

FAILURE TO DO THIS PROPERLY WILL RESULT IN A BRICK SO BOOTMII INSTALLED TO BOOT2 IS A MUST! 
I ACCEPT NO LIABILITY FOR INCORRECT USE!!


==========================
Binary Distribution Notes
==========================

This installer/removal tool will install Priiloader v0.1(rev15 from svn , which is based on preloader 0.30). 
It is also capable of updating
any previous version and can be installed over the top of an older install. There is also
a removal option that will restore your system menu and clean all traces of Priiloader
from your system.

Usage:

Press (+) to install or update Preloader.
Press (-) to remove Preloader and restore your system menu.
Hold Down (B) with any of the above options to use IOS249.
Press (HOME) to chicken out and quit the installer!

To access Priiloader and configure it please hold (RESET) during the boot cycle.

Requirements:

You need the ios used to installed patched with ES_DIverify & trucha ( in most cases you ran the installer from
Homebrew channel so that should be IOS36)

Disclaimer:

This tool comes without any warranties and we accept no liability if you turn your wii into a
paper weight with it. I have tested this tool with 100% success on 23+ WII's. Hopefully you
will have no serious issues with it!

Kudos:

Crediar (base source preloader 0.30)
DacoTaco (additional mods)
Phpgeek (additional mods & installer)



/phpgeek & DacoTaco
