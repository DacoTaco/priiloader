# About Hacks



Priiloader includes a file called `hacks_hash.ini` (see the priiloader 
subfolder), which contains a bunch of hacks that Priiloader can apply to the 
System Menu. Each hack has a name that is surrounded by square brackets 
(like `[Name]`), and then a bunch of parameters explaining the patch. 

Required parameters are `minversion=` and `maxversion=`. These two parameters 
define which System Menu versions the hack is compatible with. For example, 
a hack that only works on 4.1 in all four regions would have the parameters
`minversion=448` and `maxversion=454`. 

The parameter `amount=` used to contain the amount of patch instructions for 
this hack. It is no longer read by current versions of Priiloader, but it 
can't hurt to add it anyways. 

After the versions and amounts are defined, the hack includes definitions for 
the actual data to patch. For each patch there'll either be an `offset=` 
parameter (in order to patch at a certain memory offset), or a `hash=` 
parameter (in order to search for a certain string and use its location as 
an offset). A patch using the `hash=` parameter is preferred, as it usually 
works on multiple versions and not just on one. 

The offset or hash parameters look like this: `offset=0x81234567` or 
`hash=0x12345678,0x98765432`. The offset parameter would include the patch
at the fixed memory address `0x81234567`, the hash parameter would add the patch
at the RAM address that contains the specified bytes(`12 34 56 78 98 76 54 32`). 

In addition to these parameters there needs to be a `patch=` parameter, which
(same syntax as `hash=`) includes the bytes to write to the found location, 
the actual patch data to replace in the system menu RAM. 

Optionally, you can also write comments by using `#` at the start of the line.


**An example hack using these parameters could look like this:** 

```
[Example Hack]
maxversion=514
minversion=514
amount=2
#Example Comment
hash=0x2c030000,0x48024451
patch=0x2c03ffff,0x48024451,0x4e800020
offset=0x81200000
patch=0x9421ffd0,0x7c0802a6,0x90010034,0x38610008
```

This results in two patches being applied to a 4.3E system menu.
One at whatever address the hash value is found, and one at the fixed 
address `0x8120000`.

*The current display limit for the hack's name is 39 characters.*



## Additional master hack / sub hack functionality

If a hack contains the `require=` parameter, that means it needs another hack
to function. The `require=` parameter can contain a string identifying the
required hack. The hack that provides the required functionality must then
provide a `master=` parameter with the same parameter. 

This causes the master hack (with the `master=` parameter) to be hidden from
the hacks menu in Priiloader. It will only be enabled once one of the sub 
hacks that requires the master hack to be present is enabled. 

This can be useful if you have a hack where a large part is version-independant,
but a small additional part is different in each version. You can then create
a large hidden master hack with the independant code, and several small sub
hacks for the different System Menu versions that all require different offsets. 

An example for that functionality can be found in the <u>Remove NoCopy Save File Protection</u> hack.

```
[Remove NoCopy Save File Protection - Master]
maxversion=518
minversion=416
amount=2
master=SaveFileProtv1
hash=0x3be00001,0x48000024,0x3be00000,0x4800001c
patch=0x3BE00000,0x48000024
hash=0x28000001,0x4082001c,0x80630068,0x3880011c
patch=0x7C000000,0x4182001C
[Remove NoCopy Save File Protection]
maxversion=518
minversion=416
amount=2
require=SaveFileProtv1
hash=0x540007ff,0x41820024,0x387d164a,0x4cc63182
patch=0x7C000000,0x41820024,0x387d164a,0x4cc63182,0x801C0024,0x5400003C,0x901C0024,0x48000018
hash=0x4803e8fd
patch=0x38600001
```

---

# Currently Supported Hacks


<details>
    <summary><b>Block Disc Updates (Wii, Mini)</b></summary>
    
Removes the “Wii System Update” screen included with some games that force you to update the system before playing the game.
Games with newer versions of the system menu/IOSs than what you have installed will not force you to update. 
Useful for staying on your current system menu and playing the most recent games without having to update.
</details>

<details>
    <summary><b>Block Online Updates</b></summary>
    
Disables updating your Wii. Updates will fail with error 32007.
Useful for staying on your current system menu and playing the most recent games without having to update.
</details>

<details>
    <summary><b>Auto-Press A at Health Screen</b></summary>

Automatically presses the A Button to get past the initial “Health and Safety” screen.
</details>

<details>
    <summary><b>Replace Health Screen with Backmenu</b></summary>

It no longer displays the health warning screen, so no need to press A, it will go straight to the channels screen.
</details>

<details>
    <summary><b>Skip Health Screen</b></summary>
    
Replace Health Screen with Backmenu hack alternative for System Menu 1.0 Only. Warmboot on 1.0 just skips the Health Screen, there’s no Backmenu animation for it to use instead.
</details>

<details>
    <summary><b>Move Disc Channel</b></summary>
    
Enables moving the Disc Channel anywhere on the Wii Menu. It’s normally stuck in the top left of the first page.
You can move the disc channel around like any other channel by holding A+B with this hack.
</details>

<details>
    <summary><b>Wiimmfi Patch v4 (SM 4.1 and Up)</b></summary>

This hack will patch all games to work with Wiimmfi when run through the disc channel, making running the patcher from the HBC redundant.
</details>

<details>
    <summary><b>480p graphics fix in the system menu (Wii)</b></summary>

Fixes an issue with 480p on the Wii Menu.
[Info with pictures can be found here](https://www.retrorgb.com/wii-480p-video-bug-discovered.html)

<img width="720" alt="fidget spinner" src="https://static.wiidatabase.de/Wii-480p-Vergleich.png">
</img>

</details>

<details>
    <summary><b>Remove NoCopy Save File Protection (Wii, vWii)</b></summary>

Allows you to copy normally disallowed save files to your SD card from Data Management.
</details>

<details>
    <summary><b>Region Free EVERYTHING</b></summary>

Disables region locking for any Wii application and Disc.
</details>

<details>
    <summary><b>No System Menu Background Music</b></summary>

Disables the background music the System Menu plays.
</details>

<details>
    <summary><b>No System Menu SFX & Background Music</b></summary>

Disables System Menu sound effects and disables the background music the System Menu plays.
</details>

<details>
    <summary><b>Re-Enable Bannerbomb v2 (Wii-4.3)</b></summary>

Enables the “Bannerbomb” exploit on the Wii System Menu 4.3. Not needed when the Homebrew Channel is already installed.
</details>

<details>
    <summary><b>OSReport to USB Gecko (Slot B) (Wii)</b></summary>

Sends Wii Menu logs to a debugging device in memory card slot B.
If enabled, [OSReport to USB Gecko (GeckoOS, Slot B)] will not work.
</details>

<details>
    <summary><b>OSReport to USB Gecko (GeckoOS, Slot B) (Wii)</b></summary>

Sends Wii Menu logs to a debugging device in memory card slot B, if the Wii Menu is launched by Gecko OS.
</details>

<details>
    <summary><b>Force Boot into Data Management</b></summary>

Forces direct booting to the Data Management screen skipping the main menu screen. This is useful for deleting data that causes a banner brick.
</details>

<details>
    <summary><b>Force Standard Recovery Mode</b></summary>

Automatically launches the console in recovery mode. Used to launch recovery discs, letting users unbrick their Wii systems.
</details>

<details>
    <summary><b>Remove Diagnostic Disc Check</b></summary>

Recovery Mode waits for Wii Backup Disc to be inserted. Enabling this should remove the check for Wii Backup Disc, and allow any disc/game to boot.
</details>

<details>
    <summary><b>No-Delete HAXX, JODI, DVDX, DISC, DISK, RZDx (SM 4.2 and Up)</b></summary>

Prevents deletion of The Homebrew Channel, DVDX, and Zothers, which was introduced in System Menu 4.2. 
Therefore the hack only exists for 4.2/4.3 System Menu versions.
It will Re-enable channels with these title IDs (originally blocked in system updates due to them being exploits).
</details>

<details>
    <summary><b>Force Disc Games to run under IOS[248,249,250,251]</b></summary>

Causes disc games to stop working. Some games will simply hang because IOS249 is a stub that isn’t properly handling IO, and others will show error 002 if they require IOS55 or higher, since they check which IOS is running. Useful for testing which games show error 002 without dumping them. If you installed cIOS for 249 the game should probably boot.
Force Disc Games to run under IOS248, IOS250, IOS251 were added to allow booting cIOSes usually installed in those slots.
</details>

<details>
    <summary><b>Remove Deflicker</b></summary>

Disables blur filter in System Menu. It is highly recommended when using 480p mode.
</details>

<details>
    <summary><b>Block Disc Autoboot</b></summary>

Blocks automatic booting of discs whose ID starts with 0 or 1.
(Diagnostic Discs)
</details>

<details>
    <summary><b>Unblock Service Discs</b></summary>

Unblocks discs with IDs:
- `RAAE` (Wii Startup Disc) - Added in SM 1.0
- `408x` and `410x` (Wii Backup Discs) - Added in SM 4.0
- `007E` (Auto Erase Disc) - Mini only
- `401A` (Unknown Repair Disc) - vWii only
</details>

<details>
    <summary><b>Remove IOS16 Disc Error (SM 4.0 and Up)</b></summary>

Disables the code in the System Menu that blocks all discs using IOS16.
IOS16 is exclusively used by Wii Backup Disc. IOS16 was blocked and stubbed with the release of System Menu 4.0.
</details>

<details>
    <summary><b>Mark Network Connection as Tested (Wii, vWii)</b></summary>

Marks the connection as tested, so the "Use this connection" button is clickable.
Good for changing the default connection without performing the test.
</details>

<details>
    <summary><b>Wii System Settings via Options Button (vWii)</b></summary>

Enables starting Wii System Settings when you click the Wii Options button.
</details>

<details>
    <summary><b>Create message via Calendar button (vWii, Mini)</b></summary>

Clicking on the Calendar button would open Create Message menu instead of the Calendar. 
This enables access to Memos, Wii Mail, and Address Book.
</details>

<details>
    <summary><b>Permanent vWii System Settings (vWii)</b></summary>
    
This hack would disable vWii System Settings overwrite that usually happens after vWii's reboot. Second thing it does is fix shutdown when standby mode is enabled.
If enabled [Always enable WiiConnect24 on boot] hack will not work.You can enable WC24 and Standby manually.
More info about it [HERE](./FAQ.md#how-to-use-permanent-wii-system-settings-on-vwii).
</details>

<details>
    <summary><b>Always enable WiiConnect24 on boot (vWii)</b></summary>

Enables WiiConnect24 and Standby Connection at vWii's SM start.
It doesn't work with [Permanent vWii System Settings] enabled.
</details>

<details>
    <summary><b>Fix NWC24iSetUniversalTime (vWii)</b></summary>

In the current state, it is a dynamic hack that needs to be generated for a specific region and a specific UTC time zone. This is an experimental hack created to fix vWii timestamp expiration bug. You can generate it [**HERE**](https://garyodernichts.github.io/priiloader-patch-gen/).
</details>
