# CardioKit-AlphaRelease
The public release of these and many more documents is a very active work in progress. The files currently present are enough to manufacture and operate the platform. That said, I am still working on documenting all of my 'institutional knowledge' that I know is just as necessary for making the platform operate as it should. 

Please feel free to reach out if you have an urgent application or desire to collaborate in the meantime.

## Quickstart Guide
- Order the ![CardioKit PCB R10](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/tree/master/Electronics/CardioKit-PCB-R01/Gerbers) with assembly services using the parts in the ![BOM](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/blob/master/Electronics/CardioKit-PCB-R01/BOM/CardioKit-R01-BOM.pdf). These should be 1.6mm thick and use ![ENIG](https://en.wikipedia.org/wiki/Electroless_nickel_immersion_gold) surface plating. Order quantity 1 per finished device.
- Order ![PCB Electrode Holders](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/tree/master/Electronics/Electrode-Holder-PCB-R01/Gerbers) along with the main PCB. These should be 0.6mm thick and use ![ENIG](https://en.wikipedia.org/wiki/Electroless_nickel_immersion_gold) surface plating. Order quantity 10 per finished device.
- Using FormLabs Form 2 with ![Elastic Resin](https://formlabs.com/store/form-2/materials/elastic-resin/) or a 3D printing service with access to the same, per device print one ![Flexible Sleeve](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/blob/master/Mechanicals/CardioKit-Flexible-Sleeve-R30/Production-Files/CardioKit-Flexible-Sleeve-R30.form) and eight ![Flexible Electrode Holders](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/blob/master/Mechanicals/CardioKit-Electrode-Holder-R50/Production-Files/CardioKit-Electrode-Holder-R50.form). These can be printed at the same time and also in batches of at least 2 sets at a time using the Form 2.
- Using FormLabs Form 2 with any colored resin of your choice (e.g. ![Grey Resin](https://formlabs.com/store/form-2/materials/grey-resin/) or a 3D printing service with access to the same, per device print one ![Bottom Shell](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/blob/master/Mechanicals/CardioKit-Shell-Bottom-R30/Production-Files/CardioKit-Shell-Bottom-R30.form) and one ![Top Shell](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/blob/master/Mechanicals/CardioKit-Shell-Top-R31/Production-Files/CardioKit-Shell-Top-R31.form). These can be printed at the same time and also in batches of at least 2 sets at a time using the Form 2.
- Using the Arduino IDE, Atom with PlatformIO, or your favorite C/C++ IDE import the ![CardioKit Firmware](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/tree/master/Firmware/CardioKit-R10-FW) and upload it to each device over the USB port. Note: you will need to have ![Teensyduino](https://www.pjrc.com/teensy/teensyduino.html) installed before uploading.
- Following the instructions in the readme, flash the ESP8266 module with ![esp-link](https://github.com/jeelabs/esp-link). This will require an ![FTDI cable](https://www.adafruit.com/product/4364) attached to the ESP8266 programming headers on the underside of the PCB.
- Assemble PCB, sensors, battery, vibration motor, and housing together into the completed device. (More detailed build guide coming soon).
- For local network use: Using Processing of Java Eclipse, import the ![CardioKit Host Software](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/tree/master/Software). With the device enabled, enter its IP address and run the software to automatically connect.
- For cloud use: Spin up an instance on AWS and follow the directions above. Note: you may need to bypass your network's firewall to successfully connect. (More details coming soon).
- That's it! All signals recorded by the device will be stored in a csv for post-processing at the end of each session. Example signal importation and post-processing can be seen in ![this Matlab script](https://github.com/jamminwfeynman/CardioKit-AlphaRelease/tree/master/Post-Processing).


## To-Do
- Clean-up, optimization, repackaging of firmware
- Clean-up, optimization, repackaging of cloud software
- Detailed description of performance and capabilities
- Detailed build guide
- Tips for self-assembly of pcb and sourcing of parts
- Initial commits of most of the post-processing code
- Tips for getting good results from 3D printer and description of process
- Detailed guide for extending FW/SW/HW/Mechanicals
- Much more...
