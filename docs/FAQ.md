# Priiloader  - Frequently Asked Questions

## General questions:

- **Do Remove Deflicker and 480p hack have any effect on the games?**  
Priiloader hacks only work with the System Menu.  
Currently, the only Hack that has any direct impact on games is the **[Wiimmfi Patch v4]**

- **I enabled the Region Free Everything hack, why is my game still not booting?**  
Region Free Everything does not work with every game.  
Some video modes might not work properly and some glitches can also occur.

- **Does Priiloader work with WiiU's Virtual Console Wii titles?**  
WiiU's WiiVC titles are not affected by Priiloader.

- **What options are not usable on vWii and Wii Mini (SM v46xx)?**  
`BootMii IOS` option currently has no purpose since BootMii doesn't exist for vWii and for Wii Mini (SM v46xx).  
`Light slot on Error` in Priiloader settings has no purpose too, since that feature is missing on WiiU and Mini.  
`Ignore standby` does nothing on vWii since there is no real Wii Standby Mode.

- **Where to find vWii Timestamp Fix Hack?**  
You can generate the vWii Timestamp Fix Hack for your specific vWii and timezone here:  
[<u>vWii Priiloader WC24 UTC Patch Generator</u>](https://garyodernichts.github.io/priiloader-patch-gen/)  
Paste it at the end of the `hacks_hash.ini` file.  
*Remember to generate two hacks if your country observes daylight saving time.*


## How to use Permanent Wii System Settings on vWii?

**IMPORTANT: Running WiiU Wii VC will result in a SYSCONF (system settings) overwrite.**  
***Hopefully, someone will find a solution for this in the future.***

The **[Always enable WiiConnect24 on boot]** is not needed and will not be enabled when using the **[Permanent vWii System Settings]** hack.  

Almost all of the Wii Settings stuff is working properly with **[Permanent vWii System Settings]** hack.  

**WiiConnect24**  
You can enable WiiConnect24 and Standby Connection manually.

**Standby Mode**  
The System Menu was fixed to shut down when using *Power Off* while Standby is enabled.  
Without the fix, it would just do the *Return to SM* instead of shutting down, since there is no real vWii Standby Mode on WiiU.

*Using "Power Off" when running games or other titles will still do the "Return to SM" instead of shutting down.*  
*Currently, there is no known way to fix this.*

**EULA and Country changing**  
It will enable accepting EULA for WiiUs with no NNID/PNID and allow changing of the WC24/Wii country/region.

*The Wii version of EULA (usually hidden channel) must be downloaded and installed since it is missing from vWii.*  
*(It is region-specific and available to download from NUS)*  

**Internet Settings**  
- Internet connection list gets overwritten when vWii boots. (Only the primary one is copied.)  
- IOSU directly writes the settings to `/shared2/sys/net/02/config.dat` right before booting into Wii mode.  
- Changing LAN connection settings works, but it gets overwritten on reboot.  
- Changing WiFi connection settings or searching for a new WiFi connection ends in an endless loop.

*Currently, there is no known way to fix this.*

**Wii System Update**  
If you start the Wii Update, it can end in an endless loop. (If you don't have the **[Block Online Updates]** hack enabled)  
If you don't have [<u>evWii</u>](https://github.com/GaryOderNichts/evwii) installed, and power off enabled, the only thing to do is pull out the power cord.

**Format Wii System Memory**  
Yup, it works. It will also leave hidden channels installed, and WC24 configuration too.

***All other settings work as they should.***


## Can Priiloader help you boot Wii Discs directly from the WiiU Disc icon?  

**Yes, yes it can! You can skip booting to the Wii System Menu.**  
To do that you need to:  

**Enable System Menu Hacks:**
- Force Standard Recovery Mode
- Remove Diagnostic Disc Check

**Set Priiloader Settings to:**
- Autoboot: System Menu
- Return to: anything but Autoboot or System Menu  
  (Doing that will just bootloop you into the same game over and over)
- Stop disc at startup: off


## Are there any known problems that I should be aware of?

Currently, there are only some smaller issues that can occur on Wii U...

- **vWii Only - Gecko Dump to SD bug**  
When Auto-booting to the System Menu while booting from the Aroma environment via the Wii Menu icon.  
Gecko Dumping to SD will not log properly. This issue is Aroma-related and hopefully will be fixed in the future.  

- **vWii Only - Using MyMenuify and Wii-Themer to change the System Menu theme when Priiloader is installed will cause brick**  
The app will need an update to work with Priiloader but can be used to change the theme before the Priiloader is installed.  
If you do brick, use [<u>vWii Decaffeinator</u>](https://github.com/GaryOderNichts/vWii-Decaffeinator) to unbrick. You need to reinstall only the System Menu.  
*It is recommended to use CSM-installer for the Wii System Menu Theme installation.*

- **vWii only - USB Loader GX currently has graphical glitches with the Channel Banners when Priiloader is installed**  
The developer fixed the bug, and the update for it will be released in the future.  
To load the games before the USB Loader GX update comes out, you can go to the loader's GUI settings and set `Game Window Mode` to `Rotating Disc`. (That allows you to access game settings easily and start the game.)  
