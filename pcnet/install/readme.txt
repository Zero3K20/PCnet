(R) IBM 10/100 Ethernet MC Windows NT 4.0 Network Device Driver
Copyright (C) 2024 Ryan Alswede All Rights Reserved
---------------------------------------------------------------------
Release 2/14/2024         "San Remo"           ryanalswede@gmail.com

HISTORY: 
This ethernet network adapter represents IBM's attempt to keep 
Microchannel Architecture  (MCA) relevant.  The base card uses
an Adapatec Bridge Chip to go from MCA bus to a PCI bus based daughter
card using PCI Mezzanine Card (PMC) architecture and format.  The daughter 
card is the only known daughter card to have been created by IBM.  The 
daughter card uses an AMD PCnet-FAST chipset to make a complete 10/100 
network card that AIX then operates in RS/6000 servers and workstations.  
The base card was meant to be generic for future daughter cards.  Of known AIX 
source code leaks, there is no San Remo source code contained in them.

I attempted to secure a licensing agreement with IBM to see the original
San Remo AIX device driver source code.  After a year of back 
and forth between an AIX Architect, AIX Delivery Manager and 
AIX Support Manager, IBM legal finally decided that
they would not allow any license agreement. Therefore, the secrets 
of San Remo would remain locked in archive in Austin, TX.
   
Christian Holzapfel was able to crack what the ASIC 9060R bridge chip
did by watching the MCA bus communications between AIX operating system 
and the San Remo during startup of AIX operating system on his RS/6000. 
We were able to decode how AIX set the PCI registers of the AMD chipset
and get a set of working register values for the ASIC.
What the actual registers control inside the ASIC is only known to IBM.
  
INSTALL:
Back up your data.  This is vintage computing and not everything goes
as planned.  Everything should be considered unknown and untested.

An open 32-bit MCA slot is required for the San Remo adapter install.

The San Remo installs just like any other MCA adapter using the ADF file
in this setup package and your IBM PS/2 system's reference disk. 

The driver itself installs in the Networking control panel of Windows NT
using the "Have Disk" option during network adapter install.

RECOMMANDED RESOURCE SETTINGS:
IO 0x1C00
DMA 5
IRQ 10

INSTALL ON WINDOWS NT 4.0:
Use OEMSETUP.INF with the "Have Disk" option in the Network control panel.

INSTALL ON WINDOWS 2000 / XP / SERVER 2003 AND LATER:
Windows 2000 and later do not enumerate MCA bus devices automatically.
You must install the driver manually using the new pcntn4m.inf file:

1. Open Network Connections (or Network and Dial-up Connections).
2. Select "Create a new connection" (or Advanced -> Add Network
   Component), then choose Adapter -> Have Disk.
   Alternatively, use the Add Hardware Wizard and choose to install
   hardware manually, selecting "Network adapters" as the category.
3. Browse to the driver folder and select pcntn4m.inf.
4. Select "IBM 10/100 Ethernet MCA Adapter (San Remo)" and click OK.

The driver reads the adapter resources (IO address, IRQ, DMA channel)
from the registry. Default values are:
  IO Base Address : 0x1C00
  IRQ             : 10
  DMA Channel     : 5

These defaults match the recommended MCA resource settings above.
If you configured the adapter to use different resources via the PS/2
reference disk, update the registry keys under:
  HKLM\SYSTEM\CurrentControlSet\Services\PCNTN4M\Parameters
    IOAddress   (DWORD, hex)
    Interrupt   (DWORD, decimal)
    DmaChannel  (DWORD, decimal)

TESTING NOTES:
Model 9577 Bermuda planar does not work with W95 or NT drivers.  The AMD chipset 
would not start during my tests.  You are welcome to try on your machine.

Device driver was tested on an IBM Model 9595 P90

Device driver was tested on an IBM Model 9590 P60

BAD CARDS:
Some San Remos have bad capacitors on the daughter board, so watch out for that.
Some San Remos have bad slot connectors as seen on Ardent Tool San Remo page.


