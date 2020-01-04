# What is Priiloader #

[Priiloader](http://wiibrew.org/wiki/Priiloader) is a heavily modified version of [Preloader 0.30](http://wiibrew.org/wiki/Preloader) by [DacoTaco](http://wiibrew.org/wiki/User:DacoTaco) and BadUncle.
It is an application that is loaded prior to the Wii System Menu, which allows it to fix certain kinds of bricks that leave the System Menu in a broken state (like a Bannerbrick), or to add various patches like update blockers to the System menu. 

**Quick note: Priiloader will _NOT_ save your Wii if you fucked up the System Menu IOS (on 4.3 this is IOS80. See [here](http://wiibrew.org/wiki/IOS_History) to check which IOS that is for you.**
# What has been changed since Preloader 0.30 base source? #

  * added Bootmii IOS booting option to menu and autoboot (handy for restoring and/or sneek)
  * Support for all HBC title IDs
  * removed need for ES\_DIVerify
  * killed the DVD spin bug (crediar forgot to close the dvd drive in ios)
  * re-added online updating
  * added our own installer (phpgeek's)
  * Re-added the old black theme
  * Added a check on boot so some apps can reboot/launch system menu and force priiloader to show up or start system menu
  * Added start of Wiiware/VC titles
  * HBC stub loading 
  * Hacks can either be added with their offset, or with a hash value to allow for version-agnostic patches
  * Added option to require a password at boot
  * Fixed lot and lots of bugs

> & much much more


# What do I need to install Priiloader? #

Thats the best part! All you need is a way of booting homebrew (Homebrew Channel recommended). If you have The Homebrew Channel 1.0.7 or above, you don't need any patched IOS at all! if you don't, then a patched IOS36 is required (although it is recommended to just update the HBC)

Priiloader itself needs no hacked IOS at all (hell, we recommend using unpatched IOS)

# Can I contact you guys elsewhere? #
Yes, you can! I (Daco) can be found in #priiloader and #wiidev on efnet.org, or on [Github](https://github.com/DacoTaco). 
