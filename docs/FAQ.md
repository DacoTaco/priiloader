# Priiloader  - FAQ

## General questions:

- **Do Remove Deflicker and 480p hack have any effect on the games?**  
Priiloader hacks only work with the System Menu;  
The only Hack that has any impact on games is the **[Wiimmfi Patch v4]**

- **Does Priiloader work with WiiU's Virtual Console Wii titles?**  
WiiU's WiiVC titles are not affected by Priiloader.

- **Options not usable on vWii and Wii Mini(SM v46xx)**  
`BootMii IOS` - BootMii IOS doesn't exist for vWii and for Wii Mini(SM v46xx).  
`Ignore standby`  and `Light slot on Error` in Priiloader settings have no purpose too, since those features are missing.

- **Where to find vWii Timestamp Fix Hack?**  
You can generate the vWii Timestamp Fix Hack for your specific vWii and timezone here:  
[<u>vWii Priiloader WC24 UTC Patch Generator</u>](https://garyodernichts.github.io/priiloader-patch-gen/)  
Paste it at the end of the `hacks_hash.ini` file.


## How to use Permanent Wii System Settings on vWii?

The **[Always enable WiiConnect24 on boot]** is not needed and will not be enabled when using the **[Permanent vWii System Settings]** hack.  

Almost all of the Wii Settings stuff is working properly with **[Permanent vWii System Settings]** hack.  
    
***IMPORTANT: Running WiiU Wii VC will result in a SYSCONF (system settings) overwrite.  
Hopefully, someone will find a solution for this in the future.***  

**WiiConnect24**  
You can enable WiiConnect24 and Standby Connection manually.

**Standby Mode**  
The System Menu was fixed to shut down when using *Power Off* while Standby is enabled.  
Without the fix, it would just do the *Return to SM* instead of shutting down, since there is no real vWii Standby Mode on WiiU.  
  
*Using *Power Off* when running games or other titles will still do the *Return to SM* instead of shutting down.  
Currently, there is no known way to fix this.*

**EULA and Country changing**  
The Wii version of EULA (usually hidden channel) must be downloaded and installed.  
(It is region-specific and available to download from NUS)  
It will enable accepting EULA for WiiUs with no NNID/PNID and changing the WC24/Wii country/region.

**Internet Settings**  
- Internet connection list gets overwritten when vWii boots (Only the primary one is copied)  
- IOSU directly writes the settings to `/shared2/sys/net/02/config.dat` right before booting into Wii mode.  
- Changing LAN connection settings works, but it gets overwritten on reboot.  
- Changing WiFi connection settings or searching for a new WiFi connection ends in an endless loop.

*Not much we can do here...*

**Wii System Update**  
If you start the Wii Update, it can end in an endless loop. (If you don't have the **[Block Online Updates]** hack enabled)  
If you don't have [evWii](https://github.com/GaryOderNichts/evwii) installed, and power off enabled, the only thing to do is pull out the power cord.

**Format Wii System Memory**  
Yup, it works. It will also leave hidden channels installed, and WC24 configuration too.

***All other settings work as they should.***


## Are there any known problems that I should be aware of?

- **vWii Only - Gecko Dump to SD bug**  
When Auto-booting to the System Menu while booting from Aroma via the Wii Menu icon Gecko Dumping to SD will not log properly. The current guess is that Aroma's `.json` plugin config read or write is causing this bug. 

- **vWii Only - Using MyMenuify to change the System Menu theme will cause brick.**  
The app will need an update to work with Priiloader but can be used to change the theme before the Priiloader is installed.

- **vWii only - USB Loader GX currently has graphical glitches with the Channel Banners when Priiloader is installed**  
The developer fixed the bug, and the update for it will be released in the future. To load the games before the USB Loader GX update comes out, you can go to the loader's GUI settings and set `Game Window Mode` to `Rotating Disc` (Which allows you to access game settings and start the game)
