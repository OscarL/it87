# it87

A Haiku driver for the "Environmental Controller" (EC) part of the ITE IT87xx "Super I/O" chips.

The EC manages temperature, voltages and fan speed sensors (and fan control, but that's not supported).

Should support, for now, the following Super I/O chips:
    - IT8705F / SiS 950
    - IT8718F

## History

Originally only worked for the IT8705F (and the SiS 950 clone) "Super I/O" chip of a PC-Chips M810-LR motherboard (Athlon K7 @ 900 MHz).

Aparently, I've started working on this in mid 2003 (on BeOS R5 PE), and last changes were made to it in late 2006.

It was never published because I'm awful at C/C++ (or programming in general, you could rightly say), and was afraid of breaking other people's PCs by messing with I/O ports willy nilly :-D

Then it was lost to some HDD troubles, or so I thought. I kept an image of the borked partition, and... after many failed attemps in different points in time, I finally managed to recover something I could work on.

Now (2022), it works for the IT8718F on my Biostar A760G-M2 (Athlon II X2). And hopefully I'll be able to add support for most of the rest of IT87xx chips.

## License

MIT.
