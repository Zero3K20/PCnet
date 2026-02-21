/*++

Copyright (c) 1996 ADVANCED MICRO DEVICES, INC. All Rights Reserved.
This software is unpblished and contains the trade secrets and
confidential proprietary information of AMD. Unless otherwise provided
in the Software Agreement associated herewith, it is licensed in confidence
"AS IS" and is not to be reproduced in whole or part by any means except
for backup. Use, duplication, or disclosure by the Government is subject
to the restrictions in paragraph (b) (3) (B) of the Rights in Technical
Data and Computer Software clause in DFAR 52.227-7013 (a) (Oct 1988).
Software owned by Advanced Micro Devices, Inc., 901 Thompson Place,
Sunnyvale, CA 94088.

Module Name:

   alloc.c

Abstract:

   This file contains the code for allocating and freeing adapter
   resources for the AMD Lance Ethernet controller.
   This driver conforms to the NDIS 3.0 interface.

Environment:

   Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:


--*/

#include <ndis.h>
#include <lancehrd.h>
#include <lancesft.h>

BOOLEAN
LanceAllocateAdapterMemory(
   IN PLANCE_ADAPTER Adapter
   )

/*++

Routine Description:

   This routine allocates memory for:

   - Transmit ring entries

   - Receive ring entries

   - Transmit buffers

   - Receive buffers

   - Initialization block

Arguments:

   Adapter - The adapter to allocate memory for.

Return Value:

   Returns FALSE if some memory needed for the adapter could not
   be allocated. It does NOT call LanceDeleteAdapterMemory in this
   case.

--*/

{
    ULONG Length;
    UINT  pTempVa, pTempPa;
	UINT  maxMapReg;

   #if DBG
      if (LanceDbg)    
         DbgPrint("==>LanceAllocateAdapterMemory\n");
	  if (LanceBreak)
		DbgBreakPoint();
   #endif

	// NdisQueryMapRegisterCount was removed in WDK 7600; use the default directly.
	maxMapReg = DEFAULT_MAP_REG_COUNT;
	maxMapReg = (maxMapReg<TRANSMIT_BUFFERS)?maxMapReg:TRANSMIT_BUFFERS;

   //
   // allocate shared memory in one big chunk
   //
   //
   // Memory Allocation needed for the 32-bit PCI device.
   //
   Adapter->AllocatedNonCachedMemorySize = 
	     sizeof(LANCE_TRANSMIT_DESCRIPTOR_HI)*TRANSMIT_BUFFERS+
	     sizeof(LANCE_RECEIVE_DESCRIPTOR_HI)*RECEIVE_BUFFERS + 
	     sizeof(LANCE_INIT_BLOCK_HI) + 0x40;

   Adapter->AllocatedCachedMemorySize = 
	     TRANSMIT_BUFFER_SIZE*TRANSMIT_BUFFERS+
	     RECEIVE_BUFFER_SIZE*RECEIVE_BUFFERS+0x50;
  
   // Allocate map registers.  This function has to be called
   // before calling NdisMAllocateSharedMemory
   //
   if (NdisMAllocateMapRegisters(
         Adapter->LanceMiniportHandle,
         (UINT)Adapter->LanceDmaChannel,
         TRUE, 
//         (ULONG)4,
//	      Adapter->AllocatedCachedMemorySize  //RECEIVE_BUFFER_SIZE
			maxMapReg, //TRANSMIT_BUFFERS,
			RECEIVE_BUFFER_SIZE
         ) != NDIS_STATUS_SUCCESS) {

       return FALSE;

   }
   //
   // Allocate physically contiguous non-cached memory
   //
   NdisMAllocateSharedMemory(
	     Adapter->LanceMiniportHandle,
	     (ULONG)Adapter->AllocatedNonCachedMemorySize,
	     FALSE,
	     (PVOID *)&(Adapter->SharedMemoryVa),
	     &(Adapter->SharedMemoryPa)
	     );

   if (Adapter->SharedMemoryVa == NULL)
      return FALSE;

   //
   // Allocate physically contiguous cached memory
   //
   NdisMAllocateSharedMemory(
	     Adapter->LanceMiniportHandle,
	     (ULONG)Adapter->AllocatedCachedMemorySize,
	     TRUE, 
	     (PVOID *)&(Adapter->SharedCachedMemoryVa),
	     &(Adapter->SharedCachedMemoryPa)
	     );

   if (Adapter->SharedCachedMemoryVa == NULL)
      return FALSE;
   
   //
   // Zero out allocated memory
   //
   NdisZeroMemory(
      Adapter->SharedMemoryVa,
      Adapter->AllocatedNonCachedMemorySize
      );

   NdisZeroMemory(
      Adapter->SharedCachedMemoryVa,
      Adapter->AllocatedCachedMemorySize
      );

   //
   // Make start memory address segment aligned 
   //
   pTempVa = ((ULONG)Adapter->SharedMemoryVa + 0xf) & 0xfffffff0;
   pTempPa = (NdisGetPhysicalAddressLow(Adapter->SharedMemoryPa) + 0xf ) & 0xfffffff0;

   //
   // Allocate the initialization block.
   //
   Adapter->InitializationBlock = (PLANCE_INIT_BLOCK_HI) pTempVa;

   NdisSetPhysicalAddressLow(Adapter->InitializationBlockPhysical, pTempPa);

   #if DBG
      if (LanceDbg)
	 DbgPrint("Initialization block: V = %lx, P = %lx\n", pTempVa, pTempPa);
   #endif    
					  
   Length = 0x20;
   pTempVa += Length;
   pTempPa += Length;

   //
   // Allocate the transmit ring descriptors.
   //
   Adapter->TransmitDescriptorRing = (PLANCE_TRANSMIT_DESCRIPTOR_HI)pTempVa;

   NdisSetPhysicalAddressLow(Adapter->TransmitDescriptorRingPhysical, pTempPa);

   #if DBG
      if (LanceDbg)
	 DbgPrint("Transmit descriptors ring: V = %lx, P = %lx\n", pTempVa, pTempPa);
   #endif    
   
   Length = sizeof(LANCE_TRANSMIT_DESCRIPTOR_HI)*TRANSMIT_BUFFERS;

   pTempVa += Length;
   pTempPa += Length;
   
   //
   // Allocate the receive ring descriptors.
   //
   Adapter->ReceiveDescriptorRing = (PLANCE_RECEIVE_DESCRIPTOR_HI) pTempVa;

   NdisSetPhysicalAddressLow(Adapter->ReceiveDescriptorRingPhysical, pTempPa);

   #if DBG
      if (LanceDbg)
	 DbgPrint("Receive descriptor ring: V = %lx, P = %lx\n", pTempVa, pTempPa);
   #endif    
   
   Length = sizeof(LANCE_RECEIVE_DESCRIPTOR_HI)*RECEIVE_BUFFERS;

   //
   // Make start memory address segment aligned
   //
   pTempVa = ((ULONG)Adapter->SharedCachedMemoryVa + 0x3f) & 0xffffffc0;
   pTempPa = (NdisGetPhysicalAddressLow(Adapter->SharedCachedMemoryPa) + 0x3f ) & 0xffffffc0;

   //
   // Set the transmit buffer pointers
   //
   Adapter->TransmitBufferPointer = (PCHAR)pTempVa;
   NdisSetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical, pTempPa);

   #if DBG
      if (LanceDbg)
	 DbgPrint("Transmit buffer: V = %lx, P = %lx\n", pTempVa, pTempPa);
   #endif    
   
   Length = TRANSMIT_BUFFER_SIZE*TRANSMIT_BUFFERS;

   pTempVa += Length;
   pTempPa += Length;
   
   //
   // Set receive buffer pointers
   //
   Adapter->ReceiveBufferPointer = (PCHAR) pTempVa;
   NdisSetPhysicalAddressLow(Adapter->ReceiveBufferPointerPhysical, pTempPa);

   #if DBG
      if (LanceDbg)    
	 DbgPrint("Receive buffer: V = %lx, P = %lx\n", pTempVa, pTempPa);
   #endif    
   
   #if DBG
      if (LanceDbg)    
	 DbgPrint("<==LanceAllocateAdapterMemory\n");
   #endif    

   return TRUE;
}


VOID
LanceDeleteAdapterMemory(
   IN PLANCE_ADAPTER Adapter
   )

/*++

Routine Description:

   This routine deallocates memory for:

    - Transmit ring entries

    - Receive ring entries

    - Transmit buffers

    - Receive buffers

    - Initialization block

Arguments:

   Adapter - The adapter to deallocate memory for.

Return Value:

   None.

--*/

{
   #if DBG
     if (LanceDbg)    
		DbgPrint("==>LanceDeleteAdapterMemory\n");
	 if (LanceBreak)
		DbgBreakPoint();
   #endif    

   if (Adapter->SharedMemoryVa) {

      //
      // Free shared memory
      //
      NdisMFreeSharedMemory(
	    Adapter->LanceMiniportHandle,
	    Adapter->AllocatedNonCachedMemorySize,
	    FALSE,
	    Adapter->SharedMemoryVa,
	    Adapter->SharedMemoryPa
	    );

      NdisMFreeSharedMemory(
	    Adapter->LanceMiniportHandle,
	    Adapter->AllocatedCachedMemorySize,
		 TRUE, 
	    Adapter->SharedCachedMemoryVa,
	    Adapter->SharedCachedMemoryPa
	    );

      //
      // Free map register
      //
      NdisMFreeMapRegisters (Adapter->LanceMiniportHandle);
   }

   #if DBG
      if (LanceDbg)    
	 DbgPrint("<==LanceDeleteAdapterMemory\n");
   #endif    
}