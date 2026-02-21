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

	send.c

Abstract:

	This file contains the code for transmitting a packet.

Environment:

	Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

	$Log:   V:\network\pcnet\mini3&4\src\send.c_v  $
 * 
 *    Rev 1.53   20 Aug 1997 14:23:54   steiger
 * Further refinements to multi-send and multi-receive.
 * 
 * 
 *    Rev 1.52   12 Aug 1997 17:51:08   steiger
 * Multi-RX implemented. Copy/Compare testing has been done.

--*/


#include <ndis.h>
#include <lancehrd.h>
#include <lancesft.h>


#ifdef NDIS50_MINIPORT

#if DBG
	UINT PeakPkts = 0;
#endif

STATIC
VOID
EnableTxInts (
	IN PLANCE_ADAPTER Adapter
	);

STATIC
VOID
TxReset (
	IN PLANCE_ADAPTER Adapter
	);

#ifdef _FAILOVER
VOID
LanceRedSendPackets(
	IN PLANCE_ADAPTER Adapter,
	IN PPNDIS_PACKET PacketArray,
	IN UINT NumberOfPackets
	);
#endif //_FAILOVER

VOID
LanceSendPackets(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN PPNDIS_PACKET PacketArray,
	IN UINT NumberOfPackets
	)

/*++

Routine Description:

	LanceSendPackets () satisfies the requirements for MiniportSendPackets ()
	as defined in the NDIS specification.

Arguments:

	MiniportAdapterContext -
		The context registered with the wrapper, really a pointer to the
		adapter structure.

	PacketArray -
		A pointer to an array of descriptors for packet(s) that is(are) to be
		transmitted.

	NumberOfPackets -
		Number of valid packet descriptors in PacketArray.

Return Value:

	None.

--*/

{
	PLANCE_TRANSMIT_DESCRIPTOR		CurrentDescriptor;
	PLANCE_TRANSMIT_DESCRIPTOR_HI	CurrentDescriptorHi;
	PLANCE_ADAPTER					Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;
	UINT							TotalPacketSize;
	PCHAR							CurrentDestination;
	INT								TotalDataMoved = 0;
	ULONG							Csr0Value;
	UCHAR							CurrentDescriptorIndex;
	UCHAR							TransmitStatus;
	USHORT 						TransmitError;
	CHAR							Destination[ETH_LENGTH_OF_ADDRESS];
	UINT							AddressLength;
	NDIS_BUFFER						Buffer;
	PNDIS_PACKET_OOB_DATA			OobData;
	UINT						oldNumPkts = NumberOfPackets;
#ifdef _FAILOVER
	PLANCE_ADAPTER CurrentAdapter;
	CurrentAdapter = (ActiveAdapter == PRI)?PrimaryAdapter:SecondaryAdapter;
	if (Adapter->RedundantMode == 0)
		CurrentAdapter = Adapter;
#endif
	#if DBG
	if (LanceDbg || LanceSendDbg)
		DbgPrint("==>LanceSendPackets Qty.:%d\n", NumberOfPackets);
	if (NumberOfPackets > PeakPkts)
	{
		PeakPkts = NumberOfPackets;
	}

	#endif

#ifdef _FAILOVER
		if (Adapter->RedundantMode == 2) {
			while (NumberOfPackets--) {
				NDIS_SET_PACKET_STATUS(*PacketArray,NDIS_STATUS_SUCCESS);
				PacketArray++;
			}
	#if DBG
	if (LanceDbg)
		DbgPrint("Red 2: returned\n");
	#endif

			return; 
		}
//		else if ((Adapter->RedundantMode == 1) && (ActiveAdapter == RED)){
//			LanceRedSendPackets(SecondaryAdapter, PacketArray, NumberOfPackets);
//	#if DBG
//	if (LanceDbg)
//		DbgPrint("called RedSendPackets\n");
//	#endif
//			return;
//		}			
			
#endif // _FAILOVER


	switch (Adapter->DeviceType)
	{
		case LANCE:
		case PCNET_PCI2_A4:
		case PCNET_PCI1:
			/* If the chip not running, restart it */

			LanceReadCsr(Adapter, LANCE_CSR0, &Csr0Value);

			if ((Csr0Value & LANCE_CSR0_RUNNING) != LANCE_CSR0_RUNNING)
			{
				#if DBG
				if (LanceDbg)
				{
					DbgPrint("LanceSend: chip stopped.	CSR0: %x\n", Csr0Value);
				}
				#endif
		
				/* Restart chip */
				TxReset(Adapter);
			}
			break;
		default:
			break;
	}
	#if DBG
		if (LanceDbg)
		DbgPrint ("Enter Double Copy\n");
	#endif

	while (NumberOfPackets--)
	{
		/* Set no-reset flag for ISR routine */
		Adapter->OpFlags |= RESET_PROHIBITED;

		/* Get a pointer to the out of band data for this packet */
		OobData = NDIS_OOB_DATA_FROM_PACKET (*PacketArray);

		/* Get the current xmit descriptor. */
		CurrentDescriptorIndex	= Adapter->NextTransmitDescriptorIndex;
		if (Adapter->SwStyle == SW_STYLE_2)
		{
			CurrentDescriptorHi	= (PLANCE_TRANSMIT_DESCRIPTOR_HI)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;
			TransmitStatus		= CurrentDescriptorHi->LanceTMDFlags;
			TransmitError =	CurrentDescriptorHi->TransmitError;
		}
		else
		{
			CurrentDescriptor	= (PLANCE_TRANSMIT_DESCRIPTOR)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;
			TransmitStatus		= CurrentDescriptor->LanceTMDFlags;
			TransmitError =	CurrentDescriptor->TransmitError;
		}
		if (TransmitStatus & OWN)
		{

			#if DBG
			if (LanceDbg || LanceSendDbg)
				DbgPrint("no xmit descriptor available. Pkts send : %i\n",oldNumPkts - NumberOfPackets);
			#endif

			Adapter->OpFlags &= ~RESET_PROHIBITED;

// MJ : Disabled Tx Watermark.
//			EnableTxInts(Adapter);
			NumberOfPackets++;
			if(oldNumPkts != NumberOfPackets)
			{
				LanceReadCsr(Adapter, LANCE_CSR0, &Csr0Value);
				Csr0Value &= LANCE_CSR0_IENA;
				LanceWriteCsr(Adapter, LANCE_CSR0, Csr0Value | LANCE_CSR0_TDMD);
			}
			while (NumberOfPackets--) {
				NDIS_SET_PACKET_STATUS(*PacketArray,NDIS_STATUS_RESOURCES);
				PacketArray++;
			}
			Adapter->OpFlags |= TX_RESOURCES;
			return;
		}
		
		/* Send packet on the wire */
		/* Get the current xmit data buffer address */

		CurrentDestination = Adapter->TransmitBufferPointer +
						(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE);

	//
	// As we do not update the statistics in the ISR, we need to
	// update them here before we use this descriptor again.
	// Check if the packet completed OK, and update statistics.
	//
	if (TransmitStatus & (DERR | LANCE_TRANSMIT_DEF_ERROR))
	{
		//
		// Update general error statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_BAD];

		if (TransmitStatus & LANCE_TRANSMIT_BUF_ERROR)
			++Adapter->DmiSpecific[DMI_TXBUFF_ERR];

		if (TransmitStatus & LANCE_TRANSMIT_EXDEF_ERROR)
			++Adapter->DmiSpecific[DMI_EX_DEFER];

		if (TransmitStatus & LANCE_TRANSMIT_DEF_ERROR )
		++Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];

		if (TransmitStatus & DERR)
		{
			if (TransmitError & LANCE_TRANSMIT_UFLO_ERROR)
			{
				++Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: UFLO error detected\n");
				#endif
			}

			if (TransmitError & LANCE_TRANSMIT_LCAR_ERROR)
			{
				++Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];

			}
		
			if (TransmitError & LANCE_TRANSMIT_LCOL_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_RTRY_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_BUF_ERROR)
			{
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: BUF error detected\n");
				#endif
			}

			if (Adapter->DeviceType != PCNET_PCI2_B2 &&
			Adapter->DeviceType != PCNET_PCI3)
			{

				if ((TransmitError & LANCE_TRANSMIT_UFLO_ERROR) ||
					(TransmitError & LANCE_TRANSMIT_BUF_ERROR))
				{
	
					#if DBG
					if (LanceDbg)
					{
						DbgPrint("TXError:reset\n");
					}
					#endif
 
					if(!(Adapter->OpFlags & IN_INTERRUPT_DPC))
					{
						//
						// Disable device interrupts before reading.
						//
#ifdef _FAILOVER
						LanceInit(CurrentAdapter, FALSE);
#else
						LanceInit(Adapter, FALSE);
			#ifdef DBG
				if (LanceDbg)
				{
					DbgPrint("LanceInit is called due to send error\n");
				}
			#endif
#endif
					}
				}
			}
		}
	}
	else
	{
		//
		// Update general good statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];

		if (TransmitStatus & LANCE_TRANSMIT_ONE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
		if	(TransmitStatus & LANCE_TRANSMIT_MORE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];

		//
		// Get saved packet information for this descriptor
		//
		Adapter->DmiSpecific[DMI_TX_BYTES] += (COUNTER64) Adapter->TransmitPacketLength[CurrentDescriptorIndex];
		switch (Adapter->TransmitPacketType[CurrentDescriptorIndex])
		{
			case LANCE_DIRECTED:
				++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
				LanceAddUlongToLargeInteger(
					&Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS],
					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_MULTICAST:
				++Adapter->GeneralOptionalFrameCount[GO_MULTICAST_TRANSMITS];
				LanceAddUlongToLargeInteger(
					&Adapter->GeneralOptionalByteCount[GO_MULTICAST_TRANSMITS],
					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_BROADCAST:
				++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_TRANSMITS];
				LanceAddUlongToLargeInteger(
					&Adapter->GeneralOptionalByteCount[GO_BROADCAST_TRANSMITS],
					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
		}
	}

		/* Save packet information. */
		/* Copy address to buffer. */

		LanceMovePacket(Adapter,
			*PacketArray,
			ETH_LENGTH_OF_ADDRESS,
			Destination,
			&AddressLength,
			TRUE
			);

		ASSERT(AddressLength == ETH_LENGTH_OF_ADDRESS);

		/* Get packet flavor */

		if (ETH_IS_MULTICAST(Destination))
		{
			Adapter->TransmitPacketType[CurrentDescriptorIndex] = 
			(ETH_IS_BROADCAST(Destination)) ? LANCE_BROADCAST : LANCE_MULTICAST;

		}
		else
		{
			Adapter->TransmitPacketType[CurrentDescriptorIndex] = LANCE_DIRECTED;
		}

		/* Now fill in the adapter buffer with the data	*/
		/* from the users buffers. */

		NdisQueryPacket(
			*PacketArray,
			NULL,
			NULL,
			NULL,
			&TotalPacketSize
			);

		/* Save packet length information */

		Adapter->TransmitPacketLength[CurrentDescriptorIndex] = TotalPacketSize;

		LanceMovePacket(Adapter,
					*PacketArray,
					TotalPacketSize,
					CurrentDestination,
					&TotalDataMoved,
					TRUE
					);

		Buffer.Next = NULL;
		Buffer.Size = 0;
		Buffer.MdlFlags = 0;
		Buffer.Process = 0;
		Buffer.MappedSystemVa = CurrentDestination;
		Buffer.StartVa = CurrentDestination;
		Buffer.ByteCount = TotalPacketSize;
		Buffer.ByteOffset = 0;
		/* NdisFlushBuffer and NdisMUpdateSharedMemory are no-ops in NDIS 5.x;
		 * omitted for WDK 7600 compatibility. */
								
		if (Adapter->SwStyle == SW_STYLE_2)
		{
			CurrentDescriptorHi->LanceBufferPhysicalLow =
			LANCE_GET_LOW_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

			CurrentDescriptorHi->LanceBufferPhysicalHighL =
			LANCE_GET_HIGH_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

			CurrentDescriptorHi->LanceBufferPhysicalHighH =
			LANCE_GET_HIGH_PART_ADDRESS_H(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

			if (TotalDataMoved < LANCE_MIN_PACKET_SIZE)
			{
				CurrentDescriptorHi->ByteCount = -LANCE_MIN_PACKET_SIZE;
			}
			else
			{
				CurrentDescriptorHi->ByteCount = -TotalDataMoved;
			}

			/* Now change the ownership of the packet to Lance. */
			/* STP and ENP bits are permanently set as all packets should */
			/* fit in one buffer only. Also clear the error bits. */

			CurrentDescriptorHi->LanceTMDFlags &=
				~(LANCE_TRANSMIT_MORE_COLLISION |
				LANCE_TRANSMIT_ONE_COLLISION |
				LANCE_TRANSMIT_DEF_ERROR |
				DERR);
		
			CurrentDescriptorHi->TransmitError = 0;
		
			OobData->Status = NDIS_STATUS_SUCCESS;
			/* Set STP and ENP bits. */

			CurrentDescriptorHi->LanceTMDFlags |= (STP | ENP);

			/* Set OWN bit so chip can start transfer */

			CurrentDescriptorHi->LanceTMDFlags |= OWN;

		}
		else
		{

			if (TotalDataMoved < LANCE_MIN_PACKET_SIZE)
			{
				CurrentDescriptor->ByteCount = -LANCE_MIN_PACKET_SIZE;
			}
			else
			{
				CurrentDescriptor->ByteCount = -TotalDataMoved;
			}

			CurrentDescriptor->LanceBufferPhysicalLow =
			LANCE_GET_LOW_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE));

			CurrentDescriptor->LanceBufferPhysicalHighL =
			LANCE_GET_HIGH_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE));

			OobData->Status = NDIS_STATUS_SUCCESS;
			/* Now change the ownership of the packet to Lance. */
			/* STP and ENP bits are permanently set as all packets should */
			/* fit in one buffer only. Also clear the error bits. */

			CurrentDescriptor->LanceTMDFlags &=
				~(LANCE_TRANSMIT_MORE_COLLISION |
				LANCE_TRANSMIT_ONE_COLLISION |
				LANCE_TRANSMIT_DEF_ERROR |
				DERR);
		
			CurrentDescriptor->TransmitError = 0;
		
			/* Set STP and ENP bits. */

			CurrentDescriptor->LanceTMDFlags |= (STP | ENP);

			/* Toggle OWN bit so chip can start transfer */

			CurrentDescriptor->LanceTMDFlags |= OWN;

		}

		/* Start chip now to send packet on the wire */

//	   NdisAcquireSpinLock(&Adapter->SendLock);
//	   NdisReleaseSpinLock(&Adapter->SendLock);

		/* Increment the next available xmit descriptor index. */
// MJ : Disabled Tx Watermark.
//		if (++Adapter->TxBufsUsed == TX_INT_WATERMARK)
//		{
//			/* Enable TX interrupts */
//			EnableTxInts(Adapter);
//		}
		PacketArray++;
		if (++Adapter->NextTransmitDescriptorIndex >= TRANSMIT_BUFFERS)
		{
			Adapter->NextTransmitDescriptorIndex = 0;
		}

		#if DBG
		if (LanceDbg)
			DbgPrint("LanceSendPackets: Packet Transmitted.\n");
		#endif
	} //while

		LanceReadCsr(Adapter, LANCE_CSR0, &Csr0Value);
		Csr0Value &= LANCE_CSR0_IENA;
		LanceWriteCsr(Adapter, LANCE_CSR0, Csr0Value | LANCE_CSR0_TDMD);

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceSendPackets\n");
	#endif
}	
STATIC
VOID
TxReset (
	IN PLANCE_ADAPTER Adapter
	)
{
#ifdef _FAILOVER
	PLANCE_ADAPTER CurrentAdapter;
	CurrentAdapter = (ActiveAdapter == PRI)?PrimaryAdapter:SecondaryAdapter;
	if (Adapter->RedundantMode == 0)
		CurrentAdapter = Adapter;
#endif
	#if DBG
	if (LanceDbg)
	{
		DbgPrint("TXError:reset\n");
	}
	#endif

	if(!(Adapter->OpFlags & IN_INTERRUPT_DPC))
	{
//		Adapter->OpFlags |= RESET_IN_PROGRESS;
#ifdef _FAILOVER
		LanceInit(CurrentAdapter, FALSE);
#else
		LanceInit(Adapter, FALSE);
#endif
//		Adapter->OpFlags &= ~RESET_IN_PROGRESS;
	}
	else	/* This should never happen */
	{
		#if DBG
		if (LanceDbg)
		{
			DbgPrint("Unable to reset - IN_INTERRUPT_DPC\n");
		}
		#endif
	}
}

#else	/* start *NOT* NDIS50_MINIPORT */

NDIS_STATUS
LanceSend(
	IN NDIS_HANDLE MiniportAdapterContext,
	IN PNDIS_PACKET Packet,
	IN UINT SendFlags
	)

/*++

Routine Description:

	The LanceSend request instructs the driver to transmit a packet through
	the adapter onto the medium.	The wrapper send its request only when
	the driver is free.	So this routine can be interrupted but can not be
	retried.	No request queuing mechanism is implemented.

Arguments:

	MiniportAdapterContext - The context registered with the wrapper,
							really a pointer to the adapter

	Packet - A pointer to a descriptor for the packet that is to be
			transmitted.

	SendFlags - Optional send flags

Return Value:

	The function value is the status of the operation.

--*/

{

	PLANCE_TRANSMIT_DESCRIPTOR CurrentDescriptor;
	PLANCE_TRANSMIT_DESCRIPTOR_HI CurrentDescriptorHi;

	//
	// Holds the status that should be returned to the caller.
	//
	PLANCE_ADAPTER Adapter = (PLANCE_ADAPTER)MiniportAdapterContext;
	ULONG Csr0Value;
	UINT TotalPacketSize;

	//
	// Index to next descriptor available.
	//
	USHORT CurrentDescriptorIndex;

	//
	// Will point into the virtual address space addressed
	// by the adpter buffer.
	//
	PCHAR CurrentDestination;

	//
	// Transmit status of the packet.
	//
	USHORT TransmitStatus;

	//
	// Transmit error of the packet.
	//
	USHORT TransmitError;

	//
	// Will hold the total amount of data copied to the
	// adapter buffer.
	//
	INT TotalDataMoved = 0;

	CHAR Destination[ETH_LENGTH_OF_ADDRESS];
	UINT AddressLength;
	NDIS_BUFFER	Buffer;

#ifdef _FAILOVER
	PLANCE_ADAPTER CurrentAdapter;
	CurrentAdapter = (ActiveAdapter == PRI)?PrimaryAdapter:SecondaryAdapter;
	if (Adapter->RedundantMode == 0)
		CurrentAdapter = Adapter;
#endif
	#if DBG
		if (LanceDbg)
		DbgPrint("==>LanceSend\n");
	#endif

	if (Adapter->DeviceType != PCNET_PCI2_B2 &&
		Adapter->DeviceType != PCNET_PCI3)
	{
		//
		// If chip not running, restart it
		//
		LanceReadCsr(Adapter, LANCE_CSR0, &Csr0Value);

		if ((Csr0Value & LANCE_CSR0_RUNNING) != LANCE_CSR0_RUNNING)
		{
			#if DBG
			if (LanceDbg)
			{
				DbgPrint("LanceSend: chip stopped.	CSR0: %x\n", Csr0Value);
			}
			#endif
	
			//
			// Restart chip if time is right
			//
			if(!(Adapter->OpFlags & IN_INTERRUPT_DPC))
			{
				#if DBG
				if (LanceDbg)
				{
					DbgPrint("LanceSend: chip stopped.	CSR0: %x\n", Csr0Value);
				}
				#endif
				LanceInit(Adapter, FALSE);
			}
		}
	}

	#if DBG
		if (LanceDbg)
		DbgPrint ("Enter Double Copy\n");
	#endif

	//
	// Set no-reset flag for ISR routine
	Adapter->OpFlags |= RESET_PROHIBITED;

	//
	// Check if there is a descriptor available for this request
	//
	//
	// Get the current xmit descriptor.
	//
	CurrentDescriptorIndex = Adapter->NextTransmitDescriptorIndex;
	if (Adapter->SwStyle == SW_STYLE_2)
	{

		CurrentDescriptorHi = (PLANCE_TRANSMIT_DESCRIPTOR_HI)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;
		TransmitStatus =	CurrentDescriptorHi->LanceTMDFlags;
		TransmitError =	CurrentDescriptorHi->TransmitError;
		if (TransmitStatus & OWN)
		{
			#if DBG
			if (LanceDbg)
				DbgPrint("LanceSend routine: no xmit descriptor available.\n");
			#endif
			Adapter->OpFlags &= ~RESET_PROHIBITED;
			return NDIS_STATUS_RESOURCES;
		}
	}
	else
	{
		CurrentDescriptor = (PLANCE_TRANSMIT_DESCRIPTOR)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;
		TransmitStatus =	CurrentDescriptor->LanceTMDFlags;
		TransmitError =	CurrentDescriptor->TransmitError;

		if (TransmitStatus & OWN)
		{
			#if DBG
			if (LanceDbg)
				DbgPrint("LanceSend routine: no xmit descriptor available.\n");
			#endif
			Adapter->OpFlags &= ~RESET_PROHIBITED;
			return NDIS_STATUS_RESOURCES;
		}
	}

	///////////////////////////////////
	// Send packet on the wire
	///////////////////////////////////

	//
	// Get the current xmit data buffer address
	//
	CurrentDestination = Adapter->TransmitBufferPointer +
					(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE);

	//
	// As we do not update the statistics in the ISR, we need to
	// update them here before we use this descriptor again.
	// Check if the packet completed OK, and update statistics.
	//
	if (TransmitStatus & (DERR | LANCE_TRANSMIT_DEF_ERROR))
	{
		//
		// Update general error statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_BAD];

		if (TransmitStatus & LANCE_TRANSMIT_DEF_ERROR )
		++Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];

		if (TransmitStatus & DERR)
		{
			if (TransmitError & LANCE_TRANSMIT_UFLO_ERROR)
			{
				++Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: UFLO error detected\n");
				#endif
			}

			if (TransmitError & LANCE_TRANSMIT_LCAR_ERROR)
			{
				++Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];

			}
		
			if (TransmitError & LANCE_TRANSMIT_LCOL_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_RTRY_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_BUF_ERROR)
			{
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: BUF error detected\n");
				#endif
			}

			if (Adapter->DeviceType != PCNET_PCI2_B2 &&
			Adapter->DeviceType != PCNET_PCI3)
			{

				if ((TransmitError & LANCE_TRANSMIT_UFLO_ERROR) ||
					(TransmitError & LANCE_TRANSMIT_BUF_ERROR))
				{
	
					#if DBG
					if (LanceDbg)
					{
						DbgPrint("TXError:reset\n");
					}
					#endif
 
					if(!(Adapter->OpFlags & IN_INTERRUPT_DPC))
					{
						Adapter->OpFlags |= RESET_IN_PROGRESS;
						//
						// Disable device interrupts before reading.
						//
#ifdef _FAILOVER
						LanceInit(CurrentAdapter, FALSE);
#else
						LanceInit(Adapter, FALSE);
#endif
						Adapter->OpFlags &= ~RESET_IN_PROGRESS;
					}
				}
			}
		}
	}
	else
	{
		//
		// Update general good statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];

		if (TransmitStatus & LANCE_TRANSMIT_ONE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
		if	(TransmitStatus & LANCE_TRANSMIT_MORE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];

		//
		// Get saved packet information for this descriptor
		//
		switch (Adapter->TransmitPacketType[CurrentDescriptorIndex])
		{
			case LANCE_DIRECTED:
				++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
				LanceAddUlongToLargeInteger(
					&Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS],
					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_MULTICAST:
				++Adapter->GeneralOptionalFrameCount[GO_MULTICAST_TRANSMITS];
				LanceAddUlongToLargeInteger(
					&Adapter->GeneralOptionalByteCount[GO_MULTICAST_TRANSMITS],
					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_BROADCAST:
				++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_TRANSMITS];
				LanceAddUlongToLargeInteger(
					&Adapter->GeneralOptionalByteCount[GO_BROADCAST_TRANSMITS],
					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
		}
	}
	//
	// Save sending packet information.
	// Copy address to buffer.
	//
	LanceMovePacket(Adapter,
		Packet,
		ETH_LENGTH_OF_ADDRESS,
		Destination,
		&AddressLength,
		TRUE
		);

	ASSERT(AddressLength == ETH_LENGTH_OF_ADDRESS);

	//
	// Save packet type
	//
	if (ETH_IS_MULTICAST(Destination))
	{
		Adapter->TransmitPacketType[CurrentDescriptorIndex] = 
		(ETH_IS_BROADCAST(Destination)) ? LANCE_BROADCAST : LANCE_MULTICAST;
	}
	else
	{
		Adapter->TransmitPacketType[CurrentDescriptorIndex] = LANCE_DIRECTED;
	}

	//
	// Now fill in the adapter buffer with the data 
	// from the users buffers.
	//
	NdisQueryPacket(
		Packet,
		NULL,
		NULL,
		NULL,
		&TotalPacketSize
		);

	//
	// Save packet length information
	//
	Adapter->TransmitPacketLength[CurrentDescriptorIndex] = TotalPacketSize;

	LanceMovePacket(Adapter,
				Packet,
				TotalPacketSize,
				CurrentDestination,
				&TotalDataMoved,
				TRUE
				);

	Buffer.Next = NULL;
	Buffer.Size = 0;
	Buffer.MdlFlags = 0;
	Buffer.Process = 0;
	Buffer.MappedSystemVa = CurrentDestination;
	Buffer.StartVa = CurrentDestination;
	Buffer.ByteCount = TotalPacketSize;
	Buffer.ByteOffset = 0;

	/* NdisFlushBuffer and NdisMUpdateSharedMemory are no-ops in NDIS 5.x;
	 * omitted for WDK 7600 compatibility. */
							
	if (Adapter->SwStyle == SW_STYLE_2)
	{

		CurrentDescriptorHi->LanceBufferPhysicalLow =
		LANCE_GET_LOW_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

		CurrentDescriptorHi->LanceBufferPhysicalHighL =
		LANCE_GET_HIGH_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

		CurrentDescriptorHi->LanceBufferPhysicalHighH =
		LANCE_GET_HIGH_PART_ADDRESS_H(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

		if (TotalDataMoved < LANCE_MIN_PACKET_SIZE)
		{
			CurrentDescriptorHi->ByteCount = -LANCE_MIN_PACKET_SIZE;
		}
		else
		{
			CurrentDescriptorHi->ByteCount = -TotalDataMoved;
		}
		//
		// Now change the ownership of the packet to Lance.
		// STP and ENP bits are permanently set as all packets should
		// fit in one buffer only. Also clear the error bits.
		//
		CurrentDescriptorHi->LanceTMDFlags &=
			~(LANCE_TRANSMIT_MORE_COLLISION |
			LANCE_TRANSMIT_ONE_COLLISION |
			LANCE_TRANSMIT_DEF_ERROR |
			DERR);
	
		CurrentDescriptorHi->TransmitError = 0;
	
		//
		// Set STP and ENP bits.
		//
		CurrentDescriptorHi->LanceTMDFlags |= (STP | ENP);

		// 
		// Toggle OWN bit so chip can start transfer
		// 
		CurrentDescriptorHi->LanceTMDFlags |= OWN;

	}
	else
	{
		if (TotalDataMoved < LANCE_MIN_PACKET_SIZE)
		{
			CurrentDescriptor->ByteCount = -LANCE_MIN_PACKET_SIZE;
		}
		else
		{
			CurrentDescriptor->ByteCount = -TotalDataMoved;
		}

		CurrentDescriptor->LanceBufferPhysicalLow =
		LANCE_GET_LOW_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE));

		CurrentDescriptor->LanceBufferPhysicalHighL =
		LANCE_GET_HIGH_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE));

		//
		// Now change the ownership of the packet to Lance.
		// STP and ENP bits are permanently set as all packets should
		// fit in one buffer only. Also clear the error bits.
		//
		CurrentDescriptor->LanceTMDFlags &=
			~(LANCE_TRANSMIT_MORE_COLLISION |
			LANCE_TRANSMIT_ONE_COLLISION |
			LANCE_TRANSMIT_DEF_ERROR |
			DERR);
	
		CurrentDescriptor->TransmitError = 0;
	
		//
		// Set STP and ENP bits.
		//
		CurrentDescriptor->LanceTMDFlags |= (STP | ENP);

		// 
		// Toggle OWN bit so chip can start transfer
		// 
		CurrentDescriptor->LanceTMDFlags |= OWN;

	}

	// 
	// Start chip now to send packet on the wire
	//
	LanceReadCsr(Adapter, LANCE_CSR0, &Csr0Value);
	Csr0Value &= LANCE_CSR0_IENA;
	LanceWriteCsr(Adapter, LANCE_CSR0, Csr0Value | LANCE_CSR0_TDMD);

	//
	// Increment the next available xit descriptor index.
	//
	if (++Adapter->NextTransmitDescriptorIndex >= TRANSMIT_BUFFERS)
	{
		Adapter->NextTransmitDescriptorIndex = 0;
	}
	Adapter->OpFlags &= ~RESET_PROHIBITED;

	#if DBG
		if (LanceDbg)
		DbgPrint("LanceSendPacket: packet transmitted.\n");
	#endif

	//
	// Return good status
	//

	#if DBG
	if (LanceDbg)
		DbgPrint("<==LanceSend\n");
	#endif
	return NDIS_STATUS_SUCCESS;
}	
#endif	/* end *NOT* NDIS50_MINIPORT */

STATIC
VOID
EnableTxInts (
	IN PLANCE_ADAPTER Adapter
	)
{
	ULONG		Data;

 	LanceReadCsr(Adapter, LANCE_CSR3, &Data);
 	Data &= ~LANCE_CSR3_TINTM;
 	LanceWriteCsr(Adapter, LANCE_CSR3, Data);
}

#ifdef _FAILOVER
VOID
LanceRedSendPackets(
	IN PLANCE_ADAPTER Adapter,
	IN PPNDIS_PACKET PacketArray,
	IN UINT NumberOfPackets
	)

/*++

Routine Description:

	LanceSendPackets () satisfies the requirements for MiniportSendPackets ()
	as defined in the NDIS specification.

Arguments:

	MiniportAdapterContext -
		The context registered with the wrapper, really a pointer to the
		adapter structure.

	PacketArray -
		A pointer to an array of descriptors for packet(s) that is(are) to be
		transmitted.

	NumberOfPackets -
		Number of valid packet descriptors in PacketArray.

Return Value:

	None.

--*/

{
	PLANCE_TRANSMIT_DESCRIPTOR		CurrentDescriptor;
	PLANCE_TRANSMIT_DESCRIPTOR_HI	CurrentDescriptorHi;
	UINT							TotalPacketSize;
	PCHAR							CurrentDestination;
	INT								TotalDataMoved = 0;
	ULONG							Csr0Value;
	UCHAR							CurrentDescriptorIndex;
	UCHAR							TransmitStatus;
	USHORT 						TransmitError;
	CHAR							Destination[ETH_LENGTH_OF_ADDRESS];
	UINT							AddressLength;
	NDIS_BUFFER						Buffer;
	PNDIS_PACKET_OOB_DATA			OobData;
	UINT						oldNumPkts = NumberOfPackets;

	#if DBG
	if (LanceDbg)
		DbgPrint("==>LanceRedSendPackets Qty.:%d\n", NumberOfPackets);
	if (NumberOfPackets > PeakPkts)
	{
		PeakPkts = NumberOfPackets;
	}

	#endif

	switch (Adapter->DeviceType)
	{
		case LANCE:
		case PCNET_PCI2_A4:
		case PCNET_PCI1:
			/* If the chip not running, restart it */

			LanceReadCsr(Adapter, LANCE_CSR0, &Csr0Value);

			if ((Csr0Value & LANCE_CSR0_RUNNING) != LANCE_CSR0_RUNNING)
			{
				#if DBG
				if (LanceDbg)
				{
					DbgPrint("LanceSend: chip stopped.	CSR0: %x\n", Csr0Value);
				}
				#endif
		
				/* Restart chip */
				TxReset(Adapter);
			}
			break;
		default:
			break;
	}
	#if DBG
		if (LanceDbg)
		DbgPrint ("Enter Double Copy\n");
	#endif

	while (NumberOfPackets--)
	{
		/* Set no-reset flag for ISR routine */
		Adapter->OpFlags |= RESET_PROHIBITED;

		/* Get a pointer to the out of band data for this packet */
		OobData = NDIS_OOB_DATA_FROM_PACKET (*PacketArray);

		/* Get the current xmit descriptor. */
		CurrentDescriptorIndex	= Adapter->NextTransmitDescriptorIndex;
		if (Adapter->SwStyle == SW_STYLE_2)
		{
			CurrentDescriptorHi	= (PLANCE_TRANSMIT_DESCRIPTOR_HI)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;
			TransmitStatus		= CurrentDescriptorHi->LanceTMDFlags;
			TransmitError =	CurrentDescriptorHi->TransmitError;
		}
		else
		{
			CurrentDescriptor	= (PLANCE_TRANSMIT_DESCRIPTOR)Adapter->TransmitDescriptorRing + CurrentDescriptorIndex;
			TransmitStatus		= CurrentDescriptor->LanceTMDFlags;
			TransmitError =	CurrentDescriptor->TransmitError;
		}
		if (TransmitStatus & OWN)
		{

			#if DBG
			if (LanceDbg)
				DbgPrint("LanceSendPackets routine: no xmit descriptor available.\n");
			#endif

//			Adapter->CableDisconnected = TRUE;
//			NdisMSetPeriodicTimer (&(Adapter->CableTimer),60000);

			Adapter->OpFlags &= ~RESET_PROHIBITED;

//			EnableTxInts(Adapter);
			NumberOfPackets++;
			if(oldNumPkts != NumberOfPackets)
			{
				LanceReadCsr(Adapter, LANCE_CSR0, &Csr0Value);
				Csr0Value &= LANCE_CSR0_IENA;
				LanceWriteCsr(Adapter, LANCE_CSR0, Csr0Value | LANCE_CSR0_TDMD);
			}
			while (NumberOfPackets--) {
				NDIS_SET_PACKET_STATUS(*PacketArray,NDIS_STATUS_RESOURCES);
				PacketArray++;
			}
			Adapter->OpFlags |= TX_RESOURCES;
			return;
		}
		
		/* Send packet on the wire */
		/* Get the current xmit data buffer address */

		CurrentDestination = Adapter->TransmitBufferPointer +
						(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE);

	//
	// As we do not update the statistics in the ISR, we need to
	// update them here before we use this descriptor again.
	// Check if the packet completed OK, and update statistics.
	//
	if (TransmitStatus & (DERR | LANCE_TRANSMIT_DEF_ERROR))
	{
		//
		// Update general error statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_BAD];

		if (TransmitStatus & LANCE_TRANSMIT_BUF_ERROR)
			++Adapter->DmiSpecific[DMI_TXBUFF_ERR];

		if (TransmitStatus & LANCE_TRANSMIT_EXDEF_ERROR)
			++Adapter->DmiSpecific[DMI_EX_DEFER];

		if (TransmitStatus & LANCE_TRANSMIT_DEF_ERROR )
		++Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];

		if (TransmitStatus & DERR)
		{
			if (TransmitError & LANCE_TRANSMIT_UFLO_ERROR)
			{
				++Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: UFLO error detected\n");
				#endif
			}

			if (TransmitError & LANCE_TRANSMIT_LCAR_ERROR)
			{
				++Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];

			}
		
			if (TransmitError & LANCE_TRANSMIT_LCOL_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_RTRY_ERROR)
				++Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			if (TransmitError & LANCE_TRANSMIT_BUF_ERROR)
			{
				#if DBG
				if (LanceDbg)
					DbgPrint("Transmit: BUF error detected\n");
				#endif
			}

			if (Adapter->DeviceType != PCNET_PCI2_B2 &&
			Adapter->DeviceType != PCNET_PCI3)
			{

				if ((TransmitError & LANCE_TRANSMIT_UFLO_ERROR) ||
					(TransmitError & LANCE_TRANSMIT_BUF_ERROR))
				{
	
					#if DBG
					if (LanceDbg)
					{
						DbgPrint("TXError:reset\n");
					}
					#endif
 
					if(!(Adapter->OpFlags & IN_INTERRUPT_DPC))
					{
						//
						// Disable device interrupts before reading.
						//
						LanceInit(Adapter, FALSE);
					}
				}
			}
		}
	}
	else
	{
		//
		// Update general good statistics
		//
		++Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];

		if (TransmitStatus & LANCE_TRANSMIT_ONE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
		if	(TransmitStatus & LANCE_TRANSMIT_MORE_COLLISION)
			++Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];

		//
		// Get saved packet information for this descriptor
		//
		Adapter->DmiSpecific[DMI_TX_BYTES] += (COUNTER64) Adapter->TransmitPacketLength[CurrentDescriptorIndex];
		switch (Adapter->TransmitPacketType[CurrentDescriptorIndex])
		{
			case LANCE_DIRECTED:
				++Adapter->GeneralOptionalFrameCount[GO_DIRECTED_TRANSMITS];
				LanceAddUlongToLargeInteger(
					&Adapter->GeneralOptionalByteCount[GO_DIRECTED_TRANSMITS],
					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_MULTICAST:
				++Adapter->GeneralOptionalFrameCount[GO_MULTICAST_TRANSMITS];
				LanceAddUlongToLargeInteger(
					&Adapter->GeneralOptionalByteCount[GO_MULTICAST_TRANSMITS],
					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
				break;

			case LANCE_BROADCAST:
				++Adapter->GeneralOptionalFrameCount[GO_BROADCAST_TRANSMITS];
				LanceAddUlongToLargeInteger(
					&Adapter->GeneralOptionalByteCount[GO_BROADCAST_TRANSMITS],
					Adapter->TransmitPacketLength[CurrentDescriptorIndex]);
		}
	}

		/* Save packet information. */
		/* Copy address to buffer. */

		LanceMovePacket(Adapter,
			*PacketArray,
			ETH_LENGTH_OF_ADDRESS,
			Destination,
			&AddressLength,
			TRUE
			);

		ASSERT(AddressLength == ETH_LENGTH_OF_ADDRESS);

		/* Get packet flavor */

		if (ETH_IS_MULTICAST(Destination))
		{
			Adapter->TransmitPacketType[CurrentDescriptorIndex] = 
			(ETH_IS_BROADCAST(Destination)) ? LANCE_BROADCAST : LANCE_MULTICAST;

		}
		else
		{
			Adapter->TransmitPacketType[CurrentDescriptorIndex] = LANCE_DIRECTED;
		}

		/* Now fill in the adapter buffer with the data	*/
		/* from the users buffers. */

		NdisQueryPacket(
			*PacketArray,
			NULL,
			NULL,
			NULL,
			&TotalPacketSize
			);

		/* Save packet length information */

		Adapter->TransmitPacketLength[CurrentDescriptorIndex] = TotalPacketSize;

		LanceMovePacket(Adapter,
					*PacketArray,
					TotalPacketSize,
					CurrentDestination,
					&TotalDataMoved,
					TRUE
					);

		Buffer.Next = NULL;
		Buffer.Size = 0;
		Buffer.MdlFlags = 0;
		Buffer.Process = 0;
		Buffer.MappedSystemVa = CurrentDestination;
		Buffer.StartVa = CurrentDestination;
		Buffer.ByteCount = TotalPacketSize;
		Buffer.ByteOffset = 0;
		/* NdisFlushBuffer and NdisMUpdateSharedMemory are no-ops in NDIS 5.x;
		 * omitted for WDK 7600 compatibility. */
								
		if (Adapter->SwStyle == SW_STYLE_2)
		{
			CurrentDescriptorHi->LanceBufferPhysicalLow =
			LANCE_GET_LOW_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

			CurrentDescriptorHi->LanceBufferPhysicalHighL =
			LANCE_GET_HIGH_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

			CurrentDescriptorHi->LanceBufferPhysicalHighH =
			LANCE_GET_HIGH_PART_ADDRESS_H(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex	* TRANSMIT_BUFFER_SIZE));

			if (TotalDataMoved < LANCE_MIN_PACKET_SIZE)
			{
				CurrentDescriptorHi->ByteCount = -LANCE_MIN_PACKET_SIZE;
			}
			else
			{
				CurrentDescriptorHi->ByteCount = -TotalDataMoved;
			}

			/* Now change the ownership of the packet to Lance. */
			/* STP and ENP bits are permanently set as all packets should */
			/* fit in one buffer only. Also clear the error bits. */

			CurrentDescriptorHi->LanceTMDFlags &=
				~(LANCE_TRANSMIT_MORE_COLLISION |
				LANCE_TRANSMIT_ONE_COLLISION |
				LANCE_TRANSMIT_DEF_ERROR |
				DERR);
		
			CurrentDescriptorHi->TransmitError = 0;
		
			OobData->Status = NDIS_STATUS_SUCCESS;
			/* Set STP and ENP bits. */

			CurrentDescriptorHi->LanceTMDFlags |= (STP | ENP);

			/* Set OWN bit so chip can start transfer */

			CurrentDescriptorHi->LanceTMDFlags |= OWN;

		}
		else
		{

			if (TotalDataMoved < LANCE_MIN_PACKET_SIZE)
			{
				CurrentDescriptor->ByteCount = -LANCE_MIN_PACKET_SIZE;
			}
			else
			{
				CurrentDescriptor->ByteCount = -TotalDataMoved;
			}

			CurrentDescriptor->LanceBufferPhysicalLow =
			LANCE_GET_LOW_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE));

			CurrentDescriptor->LanceBufferPhysicalHighL =
			LANCE_GET_HIGH_PART_ADDRESS(
			NdisGetPhysicalAddressLow(Adapter->TransmitBufferPointerPhysical) +
			(CurrentDescriptorIndex * TRANSMIT_BUFFER_SIZE));

			OobData->Status = NDIS_STATUS_SUCCESS;
			/* Now change the ownership of the packet to Lance. */
			/* STP and ENP bits are permanently set as all packets should */
			/* fit in one buffer only. Also clear the error bits. */

			CurrentDescriptor->LanceTMDFlags &=
				~(LANCE_TRANSMIT_MORE_COLLISION |
				LANCE_TRANSMIT_ONE_COLLISION |
				LANCE_TRANSMIT_DEF_ERROR |
				DERR);
		
			CurrentDescriptor->TransmitError = 0;
		
			/* Set STP and ENP bits. */

			CurrentDescriptor->LanceTMDFlags |= (STP | ENP);

			/* Toggle OWN bit so chip can start transfer */

			CurrentDescriptor->LanceTMDFlags |= OWN;

		}

		PacketArray++;
		if (++Adapter->NextTransmitDescriptorIndex >= TRANSMIT_BUFFERS)
		{
			Adapter->NextTransmitDescriptorIndex = 0;
		}

		#if DBG
		if (LanceDbg)
			DbgPrint("LanceSendPackets: Packet Transmitted.\n");
		#endif
	} // while (NumberOfPackets --)

	/* Start chip now to send packet on the wire */
	LanceReadCsr(Adapter, LANCE_CSR0, &Csr0Value);
	Csr0Value &= LANCE_CSR0_IENA;
	LanceWriteCsr(Adapter, LANCE_CSR0, Csr0Value | LANCE_CSR0_TDMD);

	#if DBG
		if (LanceDbg)
		DbgPrint("<==LanceRedSendPackets\n");
	#endif
}	
#endif //_FAILOVER
