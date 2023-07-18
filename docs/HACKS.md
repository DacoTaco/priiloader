# Hacks

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
at the fixed memory address 0x81234567, the hash parameter would add the patch
at the RAM address that contains the specified bytes(`12 34 56 78 98 76 54 32`). 

In addition to these parameters there needs to be a `patch=` parameter, which
(same syntax as `hash=`) includes the bytes to write to the found location, 
the actual patch data to replace in the system menu RAM. 

An example hack using these parameters could look like this: 

    [Example Hack]
    maxversion=514
    minversion=514
    amount=2
    hash=0x2c030000,0x48024451
    patch=0x2c03ffff,0x48024451,0x4e800020
    offset=0x81200000
    patch=0x9421ffd0,0x7c0802a6,0x90010034,0x38610008
    
This results in two patches being applied to a 4.3E system menu.
One at whatever address the hash value is found, and one at the fixed 
address 0x8120000. 

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

An example for that functionality can be found in the Remove NoCopy Save File Protection hack.

    [Remove NoCopy Save File Protection - Master]
    maxversion=4610
    minversion=416
    amount=2
    master=SaveFileProtv1
    hash=0x3be00001,0x48000024,0x3be00000,0x4800001c
    patch=0x3BE00000,0x48000024
    hash=0x28000001,0x4082001c,0x80630068,0x3880011c
    patch=0x7C000000,0x4182001C
    [Remove NoCopy Save File Protection]
    maxversion=4610
    minversion=4610
    amount=2
    require=SaveFileProtv1
    hash=0x540007FF,0x41820024,0x387D1662,0x4CC63182
    patch=0x7C000000,0x41820024,0x387d1662,0x4cc63182,0x801C0024,0x5400003C,0x901C0024,0x48000018
    hash=0x4803EBAD
    patch=0x38600001
