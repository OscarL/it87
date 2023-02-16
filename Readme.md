# it87

A Haiku driver for the "Environmental Controller" (EC) part of the ITE IT87xx "Super I/O" chips.

The EC manages temperature, voltages and fan speed sensors (and fan control, but that's not supported).

*Might* work on the following Super I/O chips:

    - IT8705F / SiS 950
    - IT8712F
    - IT8716
    - IT8718F	Note: This one works (at least for me).
    - IT8720
    - IT8721
    - IT8726
    - IT8728
    - IT8772

## Install instructions:

- Clone repo
- `make && make driverinstall`
- `cat /dev/sensor/it87`

If not, `tail -f /var/log/syslog` should at least show if something went wrong, like:

> KERN: it87: device not found.

## History

Aparently, I've started working on this in mid 2003 (on BeOS R5 PE), and last changes were made to it in late 2006.

Originally only worked for the IT8705F (and the SiS 950 clone) "Super I/O" chip of a PC-Chips M810-LR motherboard (Athlon K7 @ 900 MHz).

It was never published because I'm awful at C/C++ (or programming in general, you could rightly say), and was afraid of breaking other people's PCs by messing with I/O ports willy nilly :-D

Then it was lost to some HDD troubles, or so I thought. I kept an image of the borked partition, and... after many failed attemps in different points in time, I finally managed to recover something I could work on.

After some massaging (in 2022), it started to work under Haiku for the IT8718F on my Biostar A760G-M2 motherboard.

Now in 2023... after 20 years, I guess it is time to release it to the public :-)

Will need others to confirm if it works for the rest of the supported chips.

## License

MIT.
