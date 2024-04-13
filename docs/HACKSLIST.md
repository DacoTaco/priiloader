# List of Priiloader Hacks

## Block Disc Updates (Wii, Mini)  
    
  Removes the “Wii System Update” screen included with some games that force you to update the system before playing the game.  
  Games that have updates for your system menu, channels and/or IOSs will not force you to update.  
  Useful for staying on your current system menu and playing the most recent games without having to update.  
  vWii System Menu does not check for `__seatholder.inf` nor `__update.inf`, but it does check for `__compat.inf`.  
  Apparently `__compat.inf` is not known to be present on any Wii or Wii U disc.  

## Block Online Updates  
  
  Disables updating your Wii. Updates will fail with error 32007.  
  Useful for staying on your current system menu and playing the most recent games without having to update.  

## Auto-Press A at Health Screen  
  
  It will automatically press the A button to get past the initial “Health and Safety” screen.  

## Replace Health Screen with Backmenu  
  
  It no longer displays the health warning screen, so no need to press the A button, it will go straight to the channels screen.  

## Skip Health Screen (SM 1.0)  
  
  Replace Health Screen with Backmenu hack alternative for System Menu 1.0 Only.  
  Warm booting in System Menu 1.0 just skips the Health Screen, there’s no Backmenu animation for it to use instead.  

## Move Disc Channel  
  
  Enables moving the Disc Channel anywhere on the Wii Menu. It’s normally stuck in the top left of the first page.  
  You can move the disc channel around like any other channel by holding A+B with this hack.  

## Wiimmfi Patch v4 (SM 4.1 and Up)  
  
  This hack will patch all games to work with Wiimmfi when run through the disc channel, making running the patcher from the HBC redundant.  
  It does not patch the WiiWare titles, only Disc games.  

## 480p graphics fix in the system menu (Wii)  
  *The hack will only fix the image in the Wii System Menu.*  
  Fixes an image display issue that can occur when using 480p display mode.  
  The bug is in analog video production and the Wii U doesn't do that, it's all digital.  
  No effect on the vWii, or the Wii Mini either since they were always produced with the hardware fix.  
  [<u>More info with pictures can be found here</u>](https://www.retrorgb.com/wii-480p-video-bug-discovered.html)  

## Remove NoCopy Save File Protection (Wii, vWii)  
  
  Allows you to copy normally disallowed save files to your SD card from Data Management.  
  It does not allow you to move save-protected games.  

## Region Free EVERYTHING  
  
  Unblocks region-locked Wii Disc Titles, Gamecube Disc Titles, and Wii System Menu Titles (WiiWare, VC, Arcade, Channels)  
  The hack does not work with every game.  
  Some video modes might not work properly and some glitches can also occur.  

## No System Menu Background Music  
  
  Disables the background music the System Menu plays.  

## No System Menu SFX & Background Music  
  
  Disables System Menu sound effects and disables the background music the System Menu plays.  

## Re-Enable Bannerbomb v2 (Wii-4.3)  
  
  Enables the “Bannerbomb” exploit on the Wii System Menu 4.3.  
  Not needed when the Homebrew Channel is already installed.  

## OSReport to USB Gecko (Slot B) (Wii)  
  
  Sends Wii Menu logs to a debugging device in memory card slot B.  
  If enabled, **[OSReport to USB Gecko (GeckoOS, Slot B)]** will not work.  

## OSReport to USB Gecko (GeckoOS, Slot B) (Wii)  
  
  Sends Wii Menu logs to a debugging device in memory card slot B, if the Wii Menu is launched by Gecko OS.  

## Force Boot into Data Management (SM 3.0 and Up)  
  
  Forces direct booting to the Data Management screen skipping the main menu screen.  
  *Hack can help with a banner brick by deleting channel data that causes it.*  

## Force Standard Recovery Mode  
  
  Automatically launches the console in recovery mode.  
  Used to launch recovery discs, letting users unbrick their Wii systems.  

## Remove Diagnostic Disc Check  
  
  Recovery Mode waits for the Wii Backup Disc to be inserted.  
  Enabling this should remove the check for Wii Backup Disc, and allow any disc/game to boot.  

## No-Delete HAXX, JODI, DVDX, DISC, DISK, RZDx (SM 4.2 and Up)  
  
  Prevents deletion of The Homebrew Channel, DVDX, and Zothers, which was introduced in the System Menu 4.2 update.  
  (Therefore the hack only exists for 4.2/4.3 System Menu versions.)  

## Force Disc Games to run under IOS[248,249,250,251]  
  
  Causes disc games to stop working. Some games will simply hang because IOS249 is a stub that isn’t properly handling IO.  
  Some games will show error 002 if they require IOS55 or higher since they check which IOS is running.  
  Useful for testing which games show error 002 without dumping them. If you installed cIOS for 249 the game should probably boot.  
  Force Disc Games to run under IOS248, IOS250, and IOS251 were added to allow booting of cIOSes usually installed in those slots.  

## Remove Deflicker  
  
  Disables blur filter in System Menu. It is highly recommended when using 480p display mode.  

## Block Disc Autoboot  
  
  Blocks automatic booting of discs whose ID starts with 0 or 1. (Diagnostic Discs)  

## Unblock Service Discs  
  
  It unblocks discs with IDs:  
  * `RAAE` (Wii Startup Disc) - Added in SM 1.0  
  * `408x` and `410x` (Wii Backup Discs) - Added in SM 4.0  
  * `007E` (Auto Erase Disc) - Mini only  
  * `401A` (Unknown Repair Disc) - vWii only  

## Remove IOS16 Disc Error (SM 4.0 and Up)  
  
  Disables the code in the System Menu that blocks all discs using IOS16. (IOS16 is exclusively used by Wii Backup Disc.)  
  *IOS16 was blocked and stubbed with the release of System Menu 4.0.*  

## Mark Network Connection as Tested (Wii, vWii)  
  
  Marks the connection as tested, so the "Use this connection" button is clickable.  
  Good for changing the default connection without performing the test.  

## Wii System Settings via Options Button (vWii)  
  
  Enables starting Wii System Settings when you click the Wii Options button.  

## Create message via Calendar button (vWii, Mini)  
  
  Clicking on the Calendar button would open the Create Message menu instead of the Calendar.  
  This enables access to Memos, Wii Mail, and Address Book.  
  *The Calendar icon will stay the same unless you apply a custom theme.*

## Permanent vWii System Settings (vWii)  
  
  This hack would disable vWii System Settings overwrite which usually happens after vWii's reboot.  
  The second thing it does is fix shutdown when standby mode is enabled.  
  If enabled **[Always enable WiiConnect24 on boot]** hack will not work.  
  (You can enable WC24 and Standby manually as you would do on Wii.)
  
  Make sure to read info about it [<u>HERE</u>](./FAQ.md#how-to-use-permanent-wii-system-settings-on-vwii) before you enable it.  

## Always enable WiiConnect24 on boot (vWii)  
  Enables WiiConnect24 and Standby Connection at vWii's SM start.  
  It doesn't work with **[Permanent vWii System Settings]** enabled.  
  It doesn't work if NNID/PNID is not present on Wii U or you have a region mismatch caused by site-generated PNID.  

## Fix NWC24iSetUniversalTime (vWii)  
  In the current state, it is a dynamic hack that needs to be generated for a specific region and a specific UTC time zone.    
  You can generate it here: [<u>vWii Priiloader WC24 UTC Patch Generator</u>](https://garyodernichts.github.io/priiloader-patch-gen/).  
  Paste it at the end of the `hacks_hash.ini` file.  
  *Remember to generate two hacks if your country observes daylight saving time.*

## Remove Pillarboxing Completely (vWii)  
  Disables pillarboxing in 16:9 mode that was forced on the 4:3 tiles.  
  Titles that have IDs starting with these letters will no longer be pillarboxed:  
  `C, D, E, F, H, J, L, M, N, P, Q, R, S, T, W, X`  

## Remove Pillarboxing from VC Titles (vWii)  
  Disables pillarboxing in 16:9 mode that was forced on the VC tiles.  
  Titles that have IDs starting with these letters will no longer be pillarboxed:  
  `C, E, F, J, L, M, N, P, Q, X`  
  
