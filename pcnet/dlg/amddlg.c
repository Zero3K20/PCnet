/*****************************************************
 Copyright(c) 1994-1996 ADVANCED MICRO DEVICES, INC. all 
 rights reserved. This software may not be reproduced,
 in whole or in part, in any form or by any means 
 whatsoever without the written permission of AMD. 
 This software is provided "AS IS".
******************************************************/

  /**************************************************************************\
  *
  *  MODULE:      THE_DLL.C
  *
  *  PURPOSE:     To provide the required (simple) entry point for a resource-
  *               only DLL.
  *
  *  FUNCTIONS:   DLLEntryPoint() - DLL entry point
  *
  *
  \**************************************************************************/

  #include <windows.h>
  #include "amddlg.h"
  CHAR * copyright = "Copyright(c) 1996 ADVANCED MICRO DEVICES, INC.";


  /**************************************************************************\
  *
  *  FUNCTION:    DLLEntryPoint
  *
  *  INPUTS:      hDLL       - handle of DLL
  *               dwReason   - indicates why DLL called
  *               lpReserved - reserved
  *
  *  RETURNS:     TRUE (always, in this example.)
  *
  *               Note that the retuRn value is used only when
  *               dwReason = DLL_PROCESS_ATTACH.
  *
  *               Normally the function would return TRUE if DLL initial-
  *               ization succeeded, or FALSE it it failed.
  *
  \**************************************************************************/

  BOOL WINAPI DLLEntryPoint (HANDLE hDLL, DWORD dwReason, LPVOID lpReserved)
 {
    return TRUE;
 }
