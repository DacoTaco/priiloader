# Priiloader Wii U Forwarder

Forwarder application to open the Priiloader menu from the Wii U side.

## Technical details

The forwarder starts the vWii mode with the magic title ID 50524949- ('PRII'-) followed by a magic word.  
This magic word is then checked by Priiloader, and cleared before booting into any titles.

## Building

Install [wut](https://github.com/devkitPro/wut) and run `make`.
