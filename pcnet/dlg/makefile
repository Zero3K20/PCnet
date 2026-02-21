#*****************************************************
# Copyright(c) 1994-1996 ADVANCED MICRO DEVICES, INC. all 
# rights reserved. This software may not be reproduced,
# in whole or in part, in any form or by any means 
# whatsoever without the written permission of AMD. 
# This software is provided "AS IS".
#*****************************************************/

!include <ntwin32.mak>

all: amddlg.dll


amddlg.obj: amddlg.c
    $(cc) $(cflags) $(cvars) amddlg.c

amddlg.rbj: amddlg.rc
    rc -r -fo amddlg.res amddlg.rc
    cvtres -$(CPU) amddlg.res -o amddlg.rbj

amddlg.dll: amddlg.def amddlg.rbj amddlg.obj
    $(link)  \
    -base:0x1C000000	 \
    -dll		 \
    -entry:DLLEntryPoint$(DLLENTRY) \
    -out:amddlg.dll	 \
    amddlg.rbj amddlg.obj $(guilibs)
