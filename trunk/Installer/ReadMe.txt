---------------------------------------------------------------
* Preloader Installer - An installation utiltiy for preloader *
---------------------------------------------------------------



==============================
Compiling / Binding Preloader
==============================

In order to use this installer with your application you must first place a "certs.bin"
in the data directory along with "preloader.app". The "preloader.app" file is your
compiled binary of the preloader source that has been bound with nand booting code. You
can obtain this from any systems menu or channel. This code must be placed at the end of
your compiled binary and potentially padded. You will also need to add the correct loading
address in the dol header, in text section 1. FAILURE TO DO THIS PROPERLY WILL RESULT IN
A BRICK SO BOOTMII INSTALLED TO BOOT2 IS A MUST! I ACCEPT NO LIABILITY FOR INCORRECT USE!!


==========================
Binary Distribution Notes
==========================

This installer/removal tool will install Preloader v0.30. It is also capable of updating
any previous version and can be installed over the top of an older install. There is also
a removal option that will restore your system menu and clean all traces of Preloader
from your system.

Usage:

Press (+) to install or update Preloader.
Press (-) to remove Preloader and restore your system menu.
Hold Down (B) with any of the above options to use IOS249.
Press (HOME) to chicken out and quit the installer!

To access Preloader and configure it please hold (RESET) during the boot cycle.

Requirements:

You will need the IOS used to boot the system menu to be patched with ES_Verify. This version of
the installer does NOT currently patch your IOS for you so please use DOP IOS or Free The Bug!!!

Disclaimer:

This tool comes without any warranties and I accept no liability if you turn your wii into a
paper weight with it. I have tested this tool with 100% success on 23+ WII's. Hopefully you
will have no serious issues with it!

Kudos:

Crediar (source)
DacoTaco (additional mods)



/phpgeek
