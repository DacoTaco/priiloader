# Priiloader  - Frequently Asked Questions

## General questions:

- **Do Remove Deflicker and 480p hack have any effect on the games?**  
Priiloader hacks only work with the System Menu.  
Currently, the only Hack that has any direct impact on games is the **[Wiimmfi Patch v4]**

- **I enabled the Region Free Everything hack, why is my game still not booting?**  
Region Free Everything does not work with every game.  
Some video modes might not work properly and some glitches can also occur.

- **What options are currently not usable on vWii and Wii Mini (SM v46xx)?**  
`Light slot on Error` in Priiloader settings has no purpose too, since that feature is missing on WiiU and Mini.  
`Ignore standby` does nothing on vWii since there is no real Wii Standby Mode.

- **Does Priiloader work with WiiU's Virtual Console Wii titles?**  
WiiU's WiiVC titles are not affected by Priiloader.

- **[Always enable WiiConnect24 on boot] is not working for me, what should I do?**  
You must have NNID/PNID account connected to your WiiU, and the account has to have a matching region with your WiiU.  
**[Permanent vWii System Settings]** method is advised. It is also advised because of better homebrew support and compatibility.

- **Where to find vWii Timestamp Fix Hack?**  
You can generate the vWii Timestamp Fix Hack for your specific vWii and timezone here:  
[<u>vWii Priiloader WC24 UTC Patch Generator</u>](https://garyodernichts.github.io/priiloader-patch-gen/)  
Paste it at the end of the `hacks_hash.ini` file.  
*Remember to generate two hacks if your country observes daylight saving time.*


## How to use Permanent Wii System Settings on vWii?  

**IMPORTANT: Running WiiU Wii VC will result in a SYSCONF (system settings) overwrite.**  
To prevent this, you need to install [<u>WiiVCLaunch</u>](https://github.com/Lynx64/WiiVCLaunch/releases) Aroma plugin and enable **Preserve SYSCONF on Wii VC title launch**.  
The **[Always enable WiiConnect24 on boot]** is not needed and will not be enabled when using the **[Permanent vWii System Settings]** hack.  
Almost all of the Wii Settings options are working properly with **[Permanent vWii System Settings]** hack.  

**WiiConnect24**  
You can enable WiiConnect24 and Standby Connection manually.

**Standby Mode**  
The System Menu was fixed to shut down when using *Power Off* while Standby is enabled.  
Without the fix, it would just do the *Return to SM* instead of shutting down, since there is no real vWii Standby Mode on WiiU.

*Using "Power Off" when running games or other titles will still do the "Return to SM" instead of shutting down.*  
*Currently, there is no known way to fix this.*

**EULA and Country changing**  
It will enable accepting EULA for WiiUs with no NNID/PNID and allow changing of the country and region.  
The Wii version of EULA (usually hidden channel) must be installed since it is missing from vWii.  
It is region-specific and available to download from NUS.   

**Internet Settings**  
The Internet connection list gets written (transferred from WiiU) just before booting to the Wii mode.  
Only the primary connection is copied, and you can change the settings in the Wii mode but it gets overwritten on the next boot.  
To prevent this, you need to install [<u>WiiVCLaunch</u>](https://github.com/Lynx64/WiiVCLaunch/releases) Aroma plugin and enable **Permanent Wii Internet Settings**.  
If you want to add a new WiFi connection to the list you should not use the *Search for an Access Point* button or it will end in an endless loop.
To add WiFi connection you must do a manual setup.  

**Wii System Update**  
If you start the Wii Update, it can end in an endless loop. (If you don't have the **[Block Online Updates]** hack enabled)  
If you don't have [<u>evWii</u>](https://github.com/GaryOderNichts/evwii) installed and **Enable 4 second power press** option enabled, the only thing to do is pull out the power cord.

**Format Wii System Memory**  
Yup, it works. It will also leave hidden channels installed, and WC24 configuration too.

***All other settings work as they should...***


## Can Priiloader help you boot Wii Discs directly from the WiiU Disc icon?  

Yes, yes it can! You can skip booting to the Wii System Menu.  
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

Currently, there are only some *smaller* issues that can occur on WiiU...

- **vWii Only - Gecko Dump to SD bug**  
When Auto-booting to the System Menu while booting from the Aroma environment via the Wii Menu icon.  
Gecko Dumping to SD will not log properly. This issue is FAT driver and Aroma-related and hopefully will be fixed.  

- **vWii Only - Using MyMenuify or Wii-Themer to change the System Menu theme when Priiloader is installed will cause brick**  
It is recommended to use [<u>CSM-installer</u>](https://github.com/Naim2000/csm-installer/releases) for the Wii System Menu Theme installation or make sure to use the latest version of MyMenuifyMod.
If you do brick by using an older version, use [<u>vWii Decaffeinator</u>](https://github.com/GaryOderNichts/vWii-Decaffeinator) to unbrick.
*(You will only need to reinstall the System Menu to unbrick!)*
