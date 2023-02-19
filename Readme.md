# it87

A Haiku driver for the "Environmental Controller" (EC) part of the ITE IT87xx "Super I/O" chips.

The EC manages temperature, voltages and fan speed sensors (and fan control, but that's not supported).

*Might* work on the following Super I/O chips:

    - IT8705F / SiS 950
    - IT8712F
    - IT8716
    - IT8718F	Note: This one works, at least for me.
    - IT8720
    - IT8721
    - IT8726
    - IT8728
    - IT8772

## Install instructions:

- Clone repo
- `make && make driverinstall`

Then, using `cat /dev/sensor/it87` should give some output like this:

```sh
> cat /dev/sensor/it87 
VIN0 :   1.280 V
VIN1 :   1.136 V
VIN2 :   3.376 V
VIN3 :   5.026 V
VIN4 :  12.160 V
VIN5 :   1.968 V
VIN6 :   1.168 V
VIN7 :   4.838 V
VBAT :   3.328 V
TEMP0: -178 °C
TEMP1:   22 °C
TEMP2:   36 °C
FAN1 : 1095 RPM
FAN2 : 1268 RPM
FAN3 :    0 RPM
[~/Desktop ]
```

If not (and if `TRACE_IT87` is defined) , `tail -f /var/log/syslog` should at least show if something went wrong, like:

```
KERN: it87: device not found.
```

or something like:

```
KERN: it87: ITE8718 found at address = 0x0e80. VENDOR_ID: 0x90 - CORE_ID: 0x12 - REV: 0x03
```

if it could find a supported chip at least.

## Notes:

Voltage readings should be more or less accurate, with the possible exception of VIN5/VIN6, if your motherboard uses those to monitor -12 and -5 volts (mine uses those for RAM and HT voltages respectively).

If a temp reading seems way off (-178 in TEMP0 above, for example), it is most likely not connected / unused.

## ToDo:

- Use an struct to hold info about each sensor (like name, flags, offset, etc).
- Update [Hardmony](https://github.com/OscarL/Hardmony) to use ioctl calls instead of parsing the text output.
- Implement limits/alarms/watchdog?
- Implement Fan control? (unlikely, as BIOS' SmartGuardian works OK for me).

## History

Apparently, I've started working on this in mid 2003 (on BeOS R5 PE), and last changes were made to it in late 2006.

Originally only worked for the IT8705F (and the SiS 950 clone) "Super I/O" chip of a PC-Chips M810-LR motherboard (Athlon K7 @ 900 MHz).

It was never published because I'm awful at C/C++ (or programming in general, you could rightly say), and was afraid of breaking other people's PCs by messing with I/O ports willy nilly :-D

Then it was lost to some HDD troubles, or so I thought. I kept an image of the borked partition, and... after many failed attemps in different points in time, I finally managed to recover something I could work on.

After some massaging (in 2022), it started to work under Haiku for the IT8718F on my Biostar A760G-M2 motherboard.

Now in 2023... after 20 years, I guess it is time to release it to the public :-)

Will need others to confirm if it works for the rest of the supported chips.

## License

MIT.
