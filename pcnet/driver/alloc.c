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
	UINT  chunk;

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
   // Memory Allocation needed for the 32-bit PCI device.
   //
   Adapter->AllocatedNonCachedMemorySize = 
	     sizeof(LANCE_TRANSMIT_DESCRIPTOR_HI)*TRANSMIT_BUFFERS+
	     sizeof(LANCE_RECEIVE_DESCRIPTOR_HI)*RECEIVE_BUFFERS + 
	     sizeof(LANCE_INIT_BLOCK_HI) + 0x40;
  
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

   NdisZeroMemory(Adapter->SharedMemoryVa, Adapter->AllocatedNonCachedMemorySize);

   //
   // Allocate cached DMA buffers in chunks.
   // Each chunk (ALLOC_CHUNK_BUFFERS * BUFFER_SIZE ~ 48 KB) is reliably
   // satisfiable as physically contiguous memory on any Windows version.
   // Individual buffer addresses are computed via TX/RX_BUFFER_VA/PA macros.
   //
   for (chunk = 0; chunk < TX_CHUNK_COUNT; chunk++)
   {
      Adapter->TxChunkVa[chunk] = NULL;
      NdisMAllocateSharedMemory(
         Adapter->LanceMiniportHandle,
         (ULONG)ALLOC_CHUNK_TX_SIZE,
         TRUE,
         &Adapter->TxChunkVa[chunk],
         &Adapter->TxChunkPa[chunk]
         );
      if (Adapter->TxChunkVa[chunk] == NULL)
         return FALSE;
      NdisZeroMemory(Adapter->TxChunkVa[chunk], ALLOC_CHUNK_TX_SIZE);
   }

   for (chunk = 0; chunk < RX_CHUNK_COUNT; chunk++)
   {
      Adapter->RxChunkVa[chunk] = NULL;
      NdisMAllocateSharedMemory(
         Adapter->LanceMiniportHandle,
         (ULONG)ALLOC_CHUNK_RX_SIZE,
         TRUE,
         &Adapter->RxChunkVa[chunk],
         &Adapter->RxChunkPa[chunk]
         );
      if (Adapter->RxChunkVa[chunk] == NULL)
         return FALSE;
      NdisZeroMemory(Adapter->RxChunkVa[chunk], ALLOC_CHUNK_RX_SIZE);
   }

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
   UINT chunk;

   #if DBG
     if (LanceDbg)    
		DbgPrint("==>LanceDeleteAdapterMemory\n");
	 if (LanceBreak)
		DbgBreakPoint();
   #endif    

   if (Adapter->SharedMemoryVa) {

      //
      // Free non-cached shared memory (descriptor rings + init block)
      //
      NdisMFreeSharedMemory(
	    Adapter->LanceMiniportHandle,
	    Adapter->AllocatedNonCachedMemorySize,
	    FALSE,
	    Adapter->SharedMemoryVa,
	    Adapter->SharedMemoryPa
	    );

      /* Free TX and RX cached DMA buffer chunks */
      for (chunk = 0; chunk < TX_CHUNK_COUNT; chunk++)
      {
         if (Adapter->TxChunkVa[chunk])
            NdisMFreeSharedMemory(Adapter->LanceMiniportHandle,
               ALLOC_CHUNK_TX_SIZE, TRUE,
               Adapter->TxChunkVa[chunk], Adapter->TxChunkPa[chunk]);
      }
      for (chunk = 0; chunk < RX_CHUNK_COUNT; chunk++)
      {
         if (Adapter->RxChunkVa[chunk])
            NdisMFreeSharedMemory(Adapter->LanceMiniportHandle,
               ALLOC_CHUNK_RX_SIZE, TRUE,
               Adapter->RxChunkVa[chunk], Adapter->RxChunkPa[chunk]);
      }

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