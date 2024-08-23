
# ESP32s3 BLE Bluetooth CW Machine
___

A VS Code/PlatformIO project, Using ESPIDF's framework.
  
This ham radio project uses a BLE Bluetooth Keyboard (such as Logitech's K380s) to send Morse Code (cw), and using the audio from say your RX will decode CW too.
</p>
Main Screen:
<p align="center">  
<img src="https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/Main.jpg"  width="60%" height="60%">
</p>
Settings Screen:
<p align="center">  
<img src="https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/Settings.jpg"  width="60%" height="60%">
</p>  
 
<p dir="auto">For additional info, see YouTube video:
    <a href="https://youtu.be/Hb9m60LtSJw" rel="nofollow">https://youtu.be/Hb9m60LtSJw</a></p>
    
    
---
Main Hardware needed to replicate this project:
  
Waveshare ESP32-S3-Touch-LCD-7; SKU: 27078
  
Bluetooth Keyboard (Logitech K380s, recommended)
  
___


Additional parts needed:
  
D4184 MOS FET (1)
  
HYDZ PIEZO Buzzer (1)

2n3904 (1)

2n3906 (1)

0.1ufd (3)

10ufd (1)
 
resistors 2.2K to 15K (4)
  
___

For those who want to bypass the source code, and just "flash" your ESP32, download the [ESPhomeFlasher](https://github.com/esphome/esphome-flasher/releases) and the [.bin file](https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/firmware.bin) found at these links.
  
Note: For me, using Linux Mint, the ESPhome-Flasher's 'Browse' button did NOT work. But placing the 'firmware.bin' file in the same folder as the flasher app, allowed me to just type the file name,"firmware.bin",
in the 'Firmware' cell.
  
A 2nd tip: Connect the ESP32 to your computer BEFORE launching the ESPhome-Flasher. 
___

 The Following text/references are 'out of date' and needs work. So, for the moment, ignore.

A PCB for this project can be ordered from a board manufacturer, like [JLCPCB](https://jlcpcb.com/), using the gerber files found [Here](https://github.com/jmharvey1/ESP32BtCWKeyBrd/blob/main/MiscFiles/ESP32-BT-CW-KeyBrd_2023-03-16.zip).
  
A .PDF schematic for the PCB version of this project is [Here](https://github.com/jmharvey1/ESP32BtCWKeyBrd/blob/main/MiscFiles/ESP32-BT-CW-KeyBrd_Schematic.pdf)
  
If you have access to 3D printer, the [MiscFiles](https://github.com/jmharvey1/ESP32BtCWKeyBrd/tree/main/MiscFiles)
 folder contains “.stl” files for a simple case to host your completed project. 
___
Note: The decoder audio input circuit uses the AD senor, jack J8, (GPIO6). Depending on how you elect to link to your audio source (Hardwired or Acoustic couplping) Additional external components will be needed; i.e. bias resistors & capacitor (see below for more detail), or an Amplified condensor microphone. This signal should not exceed 2Vp-p.
</p>
Keying output is done via the UART's' TX pin Header, H3-1, (GPIO43). Note: SW1 needs to be set to route this signal to header H3. "Key Down" state is represented by a 'high' (3.3v) on the TX pin. This signal is not intended to key a transmitter directly.
</p>
To effect a hardwired audio interface, a pair of 10K voltage divider reisistors can be serries connected between gnd & the jack's 3.3V pins. Connect the resistor's middle point to J3-3 (AD sensor) pin (GPIO6). Complete the interface with, a 0.1ufd DC blocking Capacitor connected to your external source (the RX's audio/speaker out) to GPIO6. Don't forget to connect the Auido source's ground to J3-2 (Gnd). 
