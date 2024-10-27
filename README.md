
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
    <a href="https://youtu.be/xPvPC13VgmA" rel="nofollow">https://youtu.be/xPvPC13VgmA</a></p>
    
    
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

For those who want to bypass the source code, and just "flash" your ESP32s3, follow the instructions found in the [notes.txt file](<https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/notes.txt>) and download these 3 files [firmware.bin](<https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/firmware.bin>) , [bootloader.bin](<https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/bootloader.bin>), & [partitions.bin](<https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/partitions.bin>) found in the [MiscFiles](<https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles>) folder of this repository.

Windows Users, you can use the above 3 files and download espressif's [Flash_Download_Tools](<https://www.espressif.com/en/support/download/other-tools>) For more detail, see [notes.txt](<https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/notes.txt>)
___
Note: The decoder audio input circuit uses the AD senor, jack J8, (GPIO6). Depending on how you elect to link to your audio source (Hardwired or Acoustic coupling) Additional external components will be needed; i.e. bias resistors & capacitor (see below for more detail), or an Amplified condensor microphone. This signal should not exceed 2Vp-p.
</p>
Keying output is done via the UART's' RX pin Header, H3-2, (GPIO44). Note: SW1 needs to be set to route this signal to header H3. "Key Down" state is represented by a 'high' (3.3v) on the RX pin. This signal is not intended to key a transmitter directly.
</p>
To create a hardwired audio interface, a pair of 10K voltage divider reisistors can be serries connected between gnd & the jack's 3.3V pins. Connect the resistor's middle point to J3-3 (AD sensor) pin (GPIO6). Complete the interface with, a 0.1ufd DC blocking Capacitor connected to your external source (the RX's audio/speaker out) to GPIO6. Don't forget to connect the Auido source's ground to J3-2 (Gnd).

___

For those who have access to 3D printer, these “.stl” files can be used to make a simple free standing case for this project, and may be suitable for other projects using this display.
  
[Case Body](<https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/WaveShareCase.stl>)

[Case Backplate](<https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/WaveShareCase-BackPlate.stl>)

</p>
Simple Case:
<p align="center">  
<img src="https://github.com/jmharvey1/ESP32S3_CW_Machine/blob/master/MiscFiles/3DprintedCase.jpg"  width="60%" height="60%">
</p>
___ 