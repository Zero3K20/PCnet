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

    request.c

Abstract:

    This is the code file for the Advanced Micro Devices LANCE
    Ethernet controller.  This driver conforms to the NDIS 3.0 interface.

Environment:

    Kernel Mode - Or whatever is the equivalent on OS/2 and DOS.

Revision History:

	$Log:   V:/network/pcnet/mini3&4/src/request.c_v  $
 *
 *    Rev 1.51   10 Jul 1997 18:18:32   steiger
 * Added  support of OID_GEN_LINK_SPEED.
 *
 *
--*/

#include <ndis.h>
#include <lancehrd.h>
#include <lancesft.h>

//for DMI
#include "amdoids.h"
#include "amddmi.h"

#define	TOPOL_OTHER					1
#define	TOPOL_10MB_ETH				2
#define	TOPOL_100MB_ETH			3
#define	TOPOL_10_100MB_ETH		4
#define	TOPOL_100MB_VG_ANYLAN	5
#define	TOPOL_4MB_TOKEN_RING		6
#define	TOPOL_16MB_TOKEN_RING	7
#define	TOPOL_16_4MB_TOKEN_RING	8
#define	TOPOL_2MB_ARCNET			9
#define	TOPOL_20MB_ARCNET			10
#define	TOPOL_FDDI					11
#define	TOPOL_ATM					12
#define	TOPOL_APPLETALK			13

ULONG LanceCsrAccessMasks[] = {LANCE_CSR_MASK0, LANCE_CSR_MASK1, LANCE_CSR_MASK2, LANCE_CSR_MASK3};
ULONG LanceBcrAccessMasks[] = {LANCE_BCR_MASK0, LANCE_BCR_MASK1, LANCE_BCR_MASK2, LANCE_BCR_MASK3};

STATIC
BOOLEAN
LanceSuspend(
	PLANCE_ADAPTER Adapter
	);

STATIC
VOID
LanceResume(
	PLANCE_ADAPTER Adapter
	);

STATIC
BOOLEAN
PrepareLanceForPortAccess(
	USHORT RegType,
	PLANCE_ADAPTER Adapter
	);

STATIC
INT
LancePortAccess(
	PLANCE_ADAPTER Adapter,
	ULONG * pData,
	USHORT AccessType
	);

STATIC
VOID
DmiRequest(
	PLANCE_ADAPTER Adapter,
	DMIREQBLOCK *ReqBlock
	);
// for DMI upto here

STATIC
VOID
LanceChangeClass(
   IN PLANCE_ADAPTER Adapter
   );

STATIC
VOID
LanceChangeAddress(
   IN PLANCE_ADAPTER Adapter
   );

STATIC
UINT
CalculateCRC(
   IN UINT NumberOfBytes,
   IN PCHAR Input
   );




NDIS_STATUS
LanceQueryInformation(
   IN NDIS_HANDLE MiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength,
   OUT PULONG BytesWritten,
   OUT PULONG BytesNeeded
   )

/*++

Routine Description:

   The LanceQuerylInformation process a Query request for
   NDIS_OIDs that are specific to a binding about the mini-port.

Arguments:

   MiniportAdapterContext - a pointer to the adapter.

   Oid - the NDIS_OID to process.

   InfoBuffer - a pointer into the NdisRequest->InformationBuffer
   into which store the result of the query.

   BytesLeft - the number of bytes left in the InformationBuffer.

   BytesNeeded - If there is not enough room in the information buffer
   then this will contain the number of bytes needed to complete the
   request.

   BytesWritten - a pointer to the number of bytes written into the
   InformationBuffer.

Return Value:

   Status

--*/

{

   PLANCE_ADAPTER Adapter = MiniportAdapterContext;
   PNDIS_OID SupportedOidArray;
   INT SupportedOids;
   PVOID SourceBuffer;
   ULONG SourceBufferLength;
   ULONG GenericUlong;
   USHORT GenericUshort;
   NDIS_STATUS Status;
   UCHAR VendorId[4];
   NDIS_OID MaskOid;

   static UCHAR VendorDescription[] = "IBM 10/100 Mbps Ethernet TX MCA Adapter";
   static NDIS_OID LanceGlobalSupportedOids[] = {
                           OID_GEN_SUPPORTED_LIST,
                           OID_GEN_HARDWARE_STATUS,
                           OID_GEN_MEDIA_SUPPORTED,
                           OID_GEN_MEDIA_IN_USE,
#ifdef NDIS50_MINIPORT
                           OID_GEN_MEDIA_CONNECT_STATUS,
									OID_GEN_MAXIMUM_SEND_PACKETS,
									OID_GEN_VENDOR_DRIVER_VERSION,
#endif
                           OID_GEN_MAXIMUM_LOOKAHEAD,
                           OID_GEN_MAXIMUM_FRAME_SIZE,
                           OID_GEN_MAC_OPTIONS,
                           OID_GEN_PROTOCOL_OPTIONS,
                           OID_GEN_LINK_SPEED,
                           OID_GEN_TRANSMIT_BUFFER_SPACE,
                           OID_GEN_RECEIVE_BUFFER_SPACE,
                           OID_GEN_TRANSMIT_BLOCK_SIZE,
                           OID_GEN_RECEIVE_BLOCK_SIZE,
                           OID_GEN_VENDOR_ID,
                           OID_GEN_VENDOR_DESCRIPTION,
                           OID_GEN_CURRENT_PACKET_FILTER,
                           OID_GEN_CURRENT_LOOKAHEAD,
                           OID_GEN_DRIVER_VERSION,
                           OID_GEN_MAXIMUM_TOTAL_SIZE,


                           OID_GEN_XMIT_OK,
                           OID_GEN_RCV_OK,
                           OID_GEN_XMIT_ERROR,
                           OID_GEN_RCV_ERROR,
                           OID_GEN_RCV_NO_BUFFER,
                           /* Optional Statistics OID */
						   OID_GEN_RCV_CRC_ERROR,
                           OID_802_3_PERMANENT_ADDRESS,
                           OID_802_3_CURRENT_ADDRESS,
                           OID_802_3_MULTICAST_LIST,
                           OID_802_3_MAXIMUM_LIST_SIZE,
                           OID_802_3_RCV_ERROR_ALIGNMENT,
                           OID_802_3_XMIT_ONE_COLLISION,
                           OID_802_3_XMIT_MORE_COLLISIONS,
                           /* Optional Statistics OID */
							OID_802_3_XMIT_DEFERRED,
							OID_802_3_XMIT_MAX_COLLISIONS,
							OID_802_3_RCV_OVERRUN,
							OID_802_3_XMIT_UNDERRUN,
//							OID_802_3_XMIT_HEARTBEAT_FAILURE,
							OID_802_3_XMIT_TIMES_CRS_LOST,
							OID_802_3_XMIT_LATE_COLLISIONS,
//for DMI
									OID_CUSTOM_DMI_CI_INIT,
									OID_CUSTOM_DMI_CI_INFO
                           };

   #ifdef DBG
      if (LanceDbg)
         DbgPrint("==>LanceQueryInformation\n");
   #endif

   SupportedOidArray = (PNDIS_OID)LanceGlobalSupportedOids;
   SupportedOids = sizeof(LanceGlobalSupportedOids)/sizeof(ULONG);

   //
   // Initialize these once, since this is the majority
   // of cases.
   //
   SourceBuffer = &GenericUlong;
   SourceBufferLength = sizeof(ULONG);

   //
   // Set default status
   //
   Status = NDIS_STATUS_SUCCESS;

   //
   // Switch on request type
   //
   switch (Oid & OID_TYPE_MASK) {

      case OID_TYPE_GENERAL_OPERATIONAL:

         switch (Oid) {

            case OID_GEN_MAC_OPTIONS:

               GenericUlong = (ULONG)(NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                                     NDIS_MAC_OPTION_RECEIVE_SERIALIZED |
                                     NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
                                     NDIS_MAC_OPTION_NO_LOOPBACK
                                     );

               break;

            case OID_GEN_SUPPORTED_LIST:

               SourceBuffer =  SupportedOidArray;
               SourceBufferLength = sizeof(LanceGlobalSupportedOids);
               break;

            case OID_GEN_HARDWARE_STATUS:

               GenericUlong = (Adapter->OpFlags & RESET_IN_PROGRESS) ?
                           NdisHardwareStatusReset : NdisHardwareStatusReady;

               break;

            case OID_GEN_MEDIA_SUPPORTED:
            case OID_GEN_MEDIA_IN_USE:

               GenericUlong = NdisMedium802_3;
               break;

            case OID_GEN_MAXIMUM_LOOKAHEAD:

               GenericUlong = LANCE_INDICATE_MAXIMUM-MAC_HEADER_SIZE;
               break;

            case OID_GEN_MAXIMUM_FRAME_SIZE:
               //
               // 1514 - 14
               //
               GenericUlong = LANCE_INDICATE_MAXIMUM-MAC_HEADER_SIZE;
               break;

            case OID_GEN_MAXIMUM_TOTAL_SIZE:

               GenericUlong = LANCE_INDICATE_MAXIMUM;

               break;

#ifdef NDIS50_MINIPORT

				case OID_GEN_MEDIA_CONNECT_STATUS:
					if (LanceReadLink (Adapter->MappedIoBaseAddress,
						 Adapter->DeviceType, Adapter))
						GenericUlong = NdisMediaStateConnected;
					else
						GenericUlong = NdisMediaStateDisconnected;
					break;

				case OID_GEN_MAXIMUM_SEND_PACKETS:
					GenericUlong = MAX_SEND_PACKETS;
					break;

				case OID_GEN_VENDOR_DRIVER_VERSION:
               GenericUshort = (LANCE_DRIVER_MAJOR_VERSION << 8) + LANCE_DRIVER_MINOR_VERSION;
               SourceBuffer = &GenericUshort;
               SourceBufferLength = sizeof(USHORT);
					break;
#endif

            case OID_GEN_LINK_SPEED:
					if (Adapter->DeviceType == PCNET_PCI3)
						LanceGetActiveMediaInfo (Adapter);
					GenericUlong = Adapter->LineSpeed * 10000; // in 100bps units
               break;

            case OID_GEN_TRANSMIT_BUFFER_SPACE:

               GenericUlong = TRANSMIT_BUFFER_SIZE * TRANSMIT_BUFFERS;
               break;

            case OID_GEN_RECEIVE_BUFFER_SPACE:

               GenericUlong = RECEIVE_BUFFER_SIZE * RECEIVE_BUFFERS;
               break;

            case OID_GEN_TRANSMIT_BLOCK_SIZE:

               GenericUlong = TRANSMIT_BUFFER_SIZE;
               break;

            case OID_GEN_RECEIVE_BLOCK_SIZE:

               GenericUlong = RECEIVE_BUFFER_SIZE;
               break;

            case OID_GEN_VENDOR_ID:

               LANCE_MOVE_MEMORY(VendorId, Adapter->PermanentNetworkAddress, 3);
               VendorId[3] = 0x0;
               SourceBuffer = VendorId;
               SourceBufferLength = sizeof(VendorId);
               break;

            case OID_GEN_VENDOR_DESCRIPTION:

               SourceBuffer = (PVOID)VendorDescription;
               SourceBufferLength = sizeof(VendorDescription);
               break;

            case OID_GEN_DRIVER_VERSION:

               GenericUshort = (LANCE_NDIS_MAJOR_VERSION << 8) + LANCE_NDIS_MINOR_VERSION;
               SourceBuffer = &GenericUshort;
               SourceBufferLength = sizeof(USHORT);
               break;

            case OID_GEN_CURRENT_LOOKAHEAD:

               GenericUlong = LANCE_INDICATE_MAXIMUM-MAC_HEADER_SIZE;

               #if DBG
               if (LanceDbg)
                  DbgPrint("Enter LanceQueryInformation: OID_GEN_CURRENT_LOOKAHEAD -> %d\n", GenericUlong);
               #endif

               break;

            case OID_GEN_CURRENT_PACKET_FILTER:

               GenericUlong = Adapter->CurrentPacketFilter;
               break;

            default:

               ASSERT(FALSE);
               Status = NDIS_STATUS_NOT_SUPPORTED;
               break;

         }

         break;

      case OID_TYPE_GENERAL_STATISTICS:

         switch (Oid) {
            case OID_GEN_XMIT_OK:

               GenericUlong = Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];
               break;

            case OID_GEN_RCV_OK:

               GenericUlong = Adapter->GeneralMandatory[GM_RECEIVE_GOOD];
               break;

            case OID_GEN_XMIT_ERROR:

               GenericUlong = Adapter->GeneralMandatory[GM_TRANSMIT_BAD];
               break;

            case OID_GEN_RCV_ERROR:

               GenericUlong = Adapter->GeneralMandatory[GM_RECEIVE_BAD];
               break;

            case OID_GEN_RCV_NO_BUFFER:

               GenericUlong = Adapter->MediaOptional[MO_RECEIVE_OVERRUN];
               break;
			/* Optional Statistics OID */
				case OID_GEN_RCV_CRC_ERROR:
               GenericUlong = Adapter->GeneralOptional[GO_RECEIVE_CRC-GO_ARRAY_START];
               break;
            default:

               ASSERT(FALSE);
               Status = NDIS_STATUS_NOT_SUPPORTED;
               break;
         }
         break;

      case OID_TYPE_802_3_OPERATIONAL:

         switch (Oid) {

            case OID_802_3_PERMANENT_ADDRESS:

               SourceBuffer = Adapter->PermanentNetworkAddress;
               SourceBufferLength = 6;
               break;

            case OID_802_3_CURRENT_ADDRESS:

               SourceBuffer = Adapter->CurrentNetworkAddress;
               SourceBufferLength = 6;
               break;

            case OID_802_3_MAXIMUM_LIST_SIZE:

               GenericUlong = LANCE_MAX_MULTICAST;
               break;

            default:

               ASSERT(FALSE);
               Status = NDIS_STATUS_NOT_SUPPORTED;
               break;

         }

         break;

      case OID_TYPE_802_3_STATISTICS:

         switch (Oid) {

            case OID_802_3_RCV_ERROR_ALIGNMENT:

               GenericUlong = Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT];
               break;

            case OID_802_3_XMIT_ONE_COLLISION:

               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
               break;

            case OID_802_3_XMIT_MORE_COLLISIONS:

               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
               break;

			case OID_802_3_XMIT_DEFERRED:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];
               break;

				case OID_802_3_XMIT_MAX_COLLISIONS:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
               break;

				case OID_802_3_RCV_OVERRUN:
               GenericUlong = Adapter->MediaOptional[MO_RECEIVE_OVERRUN];
               break;

				case OID_802_3_XMIT_UNDERRUN:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
               break;

//				case OID_802_3_XMIT_HEARTBEAT_FAILURE:
//               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
//               break;

				case OID_802_3_XMIT_TIMES_CRS_LOST:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];
               break;

				case OID_802_3_XMIT_LATE_COLLISIONS:
               GenericUlong = Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
               break;
            default:

               ASSERT(FALSE);
               Status = NDIS_STATUS_NOT_SUPPORTED;
               break;

         }

         break;

		case OID_TYPE_CUSTOM_OPER:

			switch (Oid)
			{
				case OID_CUSTOM_DMI_CI_INIT:
					GenericUlong = *((ULONG *)&(Adapter->DmiIdRev[0]));
					break;

				case OID_CUSTOM_DMI_CI_INFO:
					DmiRequest (Adapter, (DMIREQBLOCK *)InformationBuffer);
					#if DBG
					if (LanceDbg)
					{
						DbgPrint("LanceQueryInformation (DMI): Oid = %x\n", Oid);
					}
					#endif
					*BytesWritten = sizeof(DMIREQBLOCK);
					return Status;

				default:
					*BytesWritten = 0;
					break;
			}
			break;

      default:

         ASSERT(FALSE);
         Status = NDIS_STATUS_NOT_SUPPORTED;
         break;
   }

   if (Status == NDIS_STATUS_SUCCESS) {

      if (SourceBufferLength > InformationBufferLength) {

         //
         // Not enough room in InformationBuffer. Punt
         //
         *BytesNeeded = SourceBufferLength;
         Status = NDIS_STATUS_INVALID_LENGTH;

      } else {

         //
         // Store result.
         //
         LANCE_MOVE_MEMORY(InformationBuffer, SourceBuffer, SourceBufferLength);
         *BytesWritten = SourceBufferLength;

         #if DBG
            if (LanceDbg) {
               DbgPrint("LanceRequeryInformation: Oid = %x\n", Oid);
               DbgPrint("LanceRequeryInformation: return %x\n", *(PULONG)SourceBuffer);
            }
         #endif

      }
   }

   #if DBG
      if (LanceDbg)
         DbgPrint("<==LanceQueryInformation\n");
   #endif

   return Status;
}



NDIS_STATUS
LanceSetInformation(
   IN NDIS_HANDLE MiniportAdapterContext,
   IN NDIS_OID Oid,
   IN PVOID InformationBuffer,
   IN ULONG InformationBufferLength,
   OUT PULONG BytesRead,
   OUT PULONG BytesNeeded
   )

/*++

Routine Description:

   The LanceSetInformation handles a set operation for a
   single Oid

Arguments:

   MiniportAdapterContext - a pointer to the adapter.

   Oid - The OID of the set.

   InformationBuffer - Holds the data to be set.

   InformationBufferLength - The length of InformationBuffer.

   BytesRead - If the call is successful, returns the number
       of bytes read from InformationBuffer.

   BytesNeeded - If there is not enough data in InformationBuffer
       to satisfy the OID, returns the amount of storage needed.

Return Value:

   NDIS_STATUS_SUCCESS
   NDIS_STATUS_PENDING
   NDIS_STATUS_INVALID_LENGTH
   NDIS_STATUS_INVALID_OID

--*/

{

   PLANCE_ADAPTER Adapter = MiniportAdapterContext;
   PUCHAR InfoBuffer = (PUCHAR)InformationBuffer;
   UCHAR NewAddressCount = (UCHAR)(InformationBufferLength / ETH_LENGTH_OF_ADDRESS);
   ULONG PacketFilter;
	USHORT Flag;

#ifdef _FAILOVER
	PLANCE_ADAPTER CurrentAdapter;
	CurrentAdapter = (ActiveAdapter == PRI)?PrimaryAdapter:SecondaryAdapter;
	if (Adapter->RedundantMode == 0)
		CurrentAdapter = Adapter;
#endif

   #if DBG
      if (LanceDbg){
         DbgPrint("==>LanceSetInformation\n");
         DbgPrint("LanceSetInformation: Oid = %x\n", Oid);
      }
   #endif

   //
   // Check for the most common OIDs
   //
   switch (Oid) {

      case OID_802_3_MULTICAST_LIST:

         //
         // Verify length
         //
         if ((InformationBufferLength % ETH_LENGTH_OF_ADDRESS) != 0) {

            *BytesRead = 0;
            *BytesNeeded = 0;
            return NDIS_STATUS_INVALID_LENGTH;

         }

         //
         // Verify number of new multicast addresses
         //
         if (NewAddressCount > LANCE_MAX_MULTICAST) {

            *BytesRead = 0;
            *BytesNeeded = 0;
            return NDIS_STATUS_MULTICAST_FULL;

         }

         //
         // Save multicast address information
         //
         Adapter->NumberOfAddresses = NewAddressCount;

         LANCE_MOVE_MEMORY ((PVOID)Adapter->MulticastAddresses, InfoBuffer, InformationBufferLength);

#ifdef _FAILOVER
			if (Adapter->RedundantMode == 2)
			{
	         *BytesRead = InformationBufferLength;
				return NDIS_STATUS_SUCCESS;
			}
#endif

//MJ
			if (Adapter->CurrentPacketFilter & (NDIS_PACKET_TYPE_ALL_MULTICAST |
															NDIS_PACKET_TYPE_PROMISCUOUS))
			{

		   #if DBG
      		if (LanceFilterDbg)
		         DbgPrint("Returned since current filter is %x\n",Adapter->CurrentPacketFilter);
		   #endif
				return NDIS_STATUS_SUCCESS;
			}

         //
         // Change init-block with new multicast addresses
         //

		   #if DBG
      		if (LanceFilterDbg)
		         DbgPrint("New multicast address count is %i\n",NewAddressCount);
		   #endif
         LanceChangeAddress(Adapter);

         //
         // Reload init-block
         //
#ifdef _FAILOVER
//			Flag = CurrentAdapter->OpFlags;
//			CurrentAdapter->OpFlags &= ~RESET_PROHIBITED;
			LanceInit(CurrentAdapter, FALSE);
//			CurrentAdapter->OpFlags = Flag;
#else
//			Flag = Adapter->OpFlags;
//			Adapter->OpFlags &= ~RESET_PROHIBITED;
			LanceInit(Adapter, FALSE);
//			Adapter->OpFlags = Flag;
#endif

         //
         // Set return values
         //
         *BytesRead = InformationBufferLength;

         break;

      case OID_GEN_CURRENT_PACKET_FILTER:

         //
         // Verify length
         //
         if (InformationBufferLength != 4) {

            *BytesNeeded = 4;
            *BytesRead = 0;
            return NDIS_STATUS_INVALID_LENGTH;

         }

         //
         // Now call the filter package to set the packet filter.
         //
         LANCE_MOVE_MEMORY ((PVOID)&PacketFilter, InfoBuffer, sizeof(ULONG));

         //
         // Verify bits
         //
         if (PacketFilter & (NDIS_PACKET_TYPE_SOURCE_ROUTING |
                           NDIS_PACKET_TYPE_SMT |
                           NDIS_PACKET_TYPE_MAC_FRAME |
                           NDIS_PACKET_TYPE_FUNCTIONAL |
                           NDIS_PACKET_TYPE_ALL_FUNCTIONAL |
                           NDIS_PACKET_TYPE_GROUP
                           )) {

            *BytesRead = 4;
            *BytesNeeded = 0;
		   #if DBG
      		if (LanceFilterDbg)
		         DbgPrint("Not Supported Filter type %x\n",PacketFilter);
		   #endif
            return NDIS_STATUS_NOT_SUPPORTED;

         }

         //
         // Save new packet filter
         //
         Adapter->CurrentPacketFilter = PacketFilter;

#ifdef _FAILOVER
			if (Adapter->RedundantMode == 2)
			{
         	*BytesRead = InformationBufferLength;
				return NDIS_STATUS_SUCCESS;
			}
#endif

         //
         // Change init-block with new filter
         //
		   #if DBG
      		if (LanceFilterDbg)
		         DbgPrint("New Filter Type %x\n",PacketFilter);
		   #endif
         LanceChangeClass(Adapter);

         //
         // Reload init-block
         //
#ifdef _FAILOVER
//			Flag = CurrentAdapter->OpFlags;
//			CurrentAdapter->OpFlags &= ~RESET_PROHIBITED;
			LanceInit(CurrentAdapter, FALSE);
//			CurrentAdapter->OpFlags = Flag;
#else
//			Flag = Adapter->OpFlags;
//			Adapter->OpFlags &= ~RESET_PROHIBITED;
			LanceInit(Adapter, FALSE);
//			Adapter->OpFlags = Flag;
#endif

         //
         // Set return values
         //
         *BytesRead = InformationBufferLength;

         break;

      case OID_GEN_CURRENT_LOOKAHEAD:

         //
         // No need to record requested lookahead length since we
         // always indicate the whole packet.
         //
         *BytesRead = 4;

         break;

      default:

         *BytesRead = 0;
         *BytesNeeded = 0;
         return NDIS_STATUS_INVALID_OID;

   }

   #if DBG
      if (LanceDbg)
         DbgPrint("<==LanceSetInformation\n");
   #endif

   return NDIS_STATUS_SUCCESS;

}



STATIC
VOID
LanceChangeClass(
   IN PLANCE_ADAPTER Adapter
   )

/*++

Routine Description:

   Modifies the mode register and logical address filter of Lance.

Arguments:

   Adapter - The pointer to the adapter

Return Value:

   None.

--*/

{

   //
   // Local Pointer to the Initialization Block.
   //
   PLANCE_INIT_BLOCK InitializationBlock;
   PLANCE_INIT_BLOCK_HI InitializationBlockHi;

   #if DBG
      if (LanceFilterDbg){
         DbgPrint("==>LanceChangeClass\n");
         DbgPrint("ChangeClass: CurrentPacketFilter = %x\n",
                     Adapter->CurrentPacketFilter);
      }
   #endif

   if((Adapter->BoardFound == PCI_DEV) ||
     (Adapter->BoardFound == MCA_DEV)) {

      InitializationBlockHi = (PLANCE_INIT_BLOCK_HI)Adapter->InitializationBlock;
   }
   else {
      InitializationBlock = (PLANCE_INIT_BLOCK)Adapter->InitializationBlock;
   }

   if (Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_PROMISCUOUS) {

      #if DBG
         if (LanceFilterDbg)
            DbgPrint("ChangeClass: Go promiscious.\n");
      #endif

      if((Adapter->BoardFound == PCI_DEV) ||
      (Adapter->BoardFound == MCA_DEV)) {

         InitializationBlockHi->Mode = LANCE_PROMISCIOUS_MODE;
         if(Adapter->tp)
            InitializationBlockHi->Mode |= 0x1080;
      }
      else
      {
         InitializationBlock->Mode = LANCE_PROMISCIOUS_MODE;
         if(Adapter->tp)
            InitializationBlock->Mode |= 0x1080;
      }

   } else {

      USHORT i;

      if((Adapter->BoardFound == PCI_DEV) ||
      (Adapter->BoardFound == MCA_DEV)) {

         InitializationBlockHi->Mode = LANCE_NORMAL_MODE;
         if(Adapter->tp)
            InitializationBlockHi->Mode |= 0x1080;

      }
      else
      {
         InitializationBlock->Mode = LANCE_NORMAL_MODE;
         if(Adapter->tp)
            InitializationBlock->Mode |= 0x1080;
      }

      if (Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_ALL_MULTICAST) {

         #if DBG
            if (LanceFilterDbg)
               DbgPrint("ChangeClass: Receive all multicast.\n");
         #endif

         if((Adapter->BoardFound == PCI_DEV) ||
         (Adapter->BoardFound == MCA_DEV)) {

            for (i=0; i<8; i++)
               InitializationBlockHi->LogicalAddressFilter[i] = 0xFF;
         }
         else
         {
            for (i=0; i<8; i++)
               InitializationBlock->LogicalAddressFilter[i] = 0xFF;
         }

      } else if (Adapter->CurrentPacketFilter & NDIS_PACKET_TYPE_MULTICAST) {

         #if DBG
            if (LanceFilterDbg)
               DbgPrint("ChangeClass: Receive selected multicast.\n");
         #endif

         //
         // Change address
         //
         LanceChangeAddress(Adapter);

      }
   }

   #if DBG
      if (LanceDbg)
         DbgPrint("<==LanceChangeClass\n");
   #endif
}



STATIC
VOID
LanceChangeAddress(
   IN PLANCE_ADAPTER Adapter
   )

/*++

Routine Description:

   Modifies the logical address filter.

Arguments:

   Adapter - The pointer to the adapter

Return Value:

   None.

--*/

{

   UINT i, j;

   //
   // Local Pointer to the Initialization Block.
   //
   PLANCE_INIT_BLOCK InitializationBlock;
   PLANCE_INIT_BLOCK_HI InitializationBlockHi;

   #if DBG
      if (LanceDbg)
         DbgPrint("==>LanceChangeAddress\n");
   #endif

   if((Adapter->BoardFound == PCI_DEV) ||
   (Adapter->BoardFound == MCA_DEV)) {

      InitializationBlockHi = (PLANCE_INIT_BLOCK_HI)Adapter->InitializationBlock;
		for (i=0; i<8; i++)
	      InitializationBlockHi->LogicalAddressFilter[i] = 0;
   }
   else {
      InitializationBlock = (PLANCE_INIT_BLOCK)Adapter->InitializationBlock;
		for (i=0; i<8; i++)
	      InitializationBlock->LogicalAddressFilter[i] = 0;
   }

   //
   // Loop through, copying the addresses into the CAM.
   //

   for (i = 0; i < Adapter->NumberOfAddresses; i++) {

      UCHAR HashCode;
      UCHAR FilterByte;
      UINT CRCValue;

      HashCode = 0;

      //
      // Calculate CRC value
      //
      CRCValue = CalculateCRC(ETH_LENGTH_OF_ADDRESS,
                              Adapter->MulticastAddresses[i]
                              );

      for (j = 0; j < 6; j++)
         HashCode = (HashCode << 1) + (((UCHAR)CRCValue >> j) & 0x01);

      //
      // Bits 3-5 of HashCode point to byte in address filter.
      // Bits 0-2 point to bit within that byte.
      //
      FilterByte = HashCode >> 3;

      if((Adapter->BoardFound == PCI_DEV) ||
      (Adapter->BoardFound == MCA_DEV)) {

         InitializationBlockHi->LogicalAddressFilter[FilterByte] |=
                  (1 << (HashCode & 0x07));
      }
      else
      {
         InitializationBlock->LogicalAddressFilter[FilterByte] |=
                  (1 << (HashCode & 0x07));
      }

   }

   #if DBG
      if (LanceDbg)
         DbgPrint("<==LanceChangeAddress\n");
   #endif
}



STATIC
UINT
CalculateCRC(
   IN UINT NumberOfBytes,
   IN PCHAR Input
   )

/*++

Routine Description:

   Calculates a 32 bit CRC value over the input number of bytes.

   NOTE: ZZZ This routine assumes UINTs are 32 bits.

Arguments:

   NumberOfBytes - The number of bytes in the input.

   Input - An input "string" to calculate a CRC over.

Return Value:

   A 32 bit CRC value.

--*/

{

   const UINT POLY = 0x04c11db6;
   UINT CRCValue = 0xffffffff;

   ASSERT(sizeof(UINT) == 4);

   #if DBG
      if (LanceDbg)
         DbgPrint("==>CalculateCRC\n");
   #endif

   for (; NumberOfBytes; NumberOfBytes--) {

      UINT CurrentBit;
      UCHAR CurrentByte = *Input;
      Input++;

      for (CurrentBit = 8; CurrentBit; CurrentBit--) {

         UINT CurrentCRCHigh = CRCValue >> 31;

         CRCValue <<= 1;

         if (CurrentCRCHigh ^ (CurrentByte & 0x01)) {

            CRCValue ^= POLY;
            CRCValue |= 0x00000001;

         }

         CurrentByte >>= 1;

      }
   }

   #if DBG
      if (LanceDbg)
         DbgPrint("<==CalculateCRC\n");
   #endif

   return CRCValue;

}

//For DMI
STATIC
VOID
DmiRequest(
	PLANCE_ADAPTER Adapter,
	DMIREQBLOCK *ReqBlock
	)
/*++

Routine Description:

	Action routine that will get called when a custom OID for DMI
	instrumentation is requested. This routine receives the Dmi Request
	Block structure and fills it up.

Arguments:

	ReqBlock - Pointer to the Request Block Structure passed on by
	the Component Interface.

Return Value:

	None.

--*/

{
	//
	// Counter.
	//

	USHORT i;

	//
	// Status to return.
	//

	NDIS_STATUS Status;

	//
	// The number of addresses in the Multicast List.
	//

	UINT AddressCount;

	//
	// The Query writes addresses currently associated
	// with the Netcard.
	//

	CHAR AddressBuffer[LANCE_MAX_MULTICAST][ETH_LENGTH_OF_ADDRESS];

	//
	// Index to the Multicast List.
	//

	UCHAR index = (UCHAR)ReqBlock->Value;

	//
	// Data for SRAM size
	//
	ULONG	Data;

	ReqBlock->Status = DMI_REQ_SUCCESS;
	switch (ReqBlock->Opcode)
	{

		case DMI_OPCODE_GET_PERM_ADDRESS:
			for (i = 0; i < 6; i++)
			{
				ReqBlock->addr[i] = (UCHAR)Adapter->PermanentNetworkAddress[i];
			}
			break;

		case DMI_OPCODE_GET_CURR_ADDRESS:
			for (i = 0; i < 6; i++)
			{
				ReqBlock->addr[i] = Adapter->CurrentNetworkAddress[i];
			}
			break;

		case DMI_OPCODE_GET_TOTAL_TX_PKTS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_TRANSMIT_GOOD];
			break;

		case DMI_OPCODE_GET_TOTAL_TX_BYTES:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_TX_BYTES];
			break;

		case DMI_OPCODE_GET_TOTAL_RX_PKTS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_RECEIVE_GOOD];
			break;

		case DMI_OPCODE_GET_TOTAL_RX_BYTES:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_RX_BYTES];
			break;

		case DMI_OPCODE_GET_TOTAL_TX_ERRS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_TRANSMIT_BAD]+
			Adapter->DmiSpecific[DMI_CSR0_BABL]+
			Adapter->MediaOptional[MO_TRANSMIT_HEARTBEAT_FAILURE];
			break;

		case DMI_OPCODE_GET_TOTAL_RX_ERRS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_RECEIVE_BAD]+
			Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER];
			break;

		case DMI_OPCODE_GET_TOTAL_HOST_ERRS:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_CSR0_BABL]+
			Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER]+
			Adapter->DmiSpecific[DMI_CSR0_MERR]+
			Adapter->MediaOptional[MO_RECEIVE_OVERRUN]+
			Adapter->DmiSpecific[DMI_TXBUFF_ERR]+
			Adapter->DmiSpecific[DMI_RXBUFF_ERR]+
			Adapter->MediaOptional[MO_TRANSMIT_DEFERRED]+
			Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
			break;

		case DMI_OPCODE_GET_TOTAL_WIRE_ERRS:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_HEARTBEAT_FAILURE]+
			Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT]+
			Adapter->GeneralOptional[GO_RECEIVE_CRC-GO_ARRAY_START]+
			Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS]+
			Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS]+
			Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST]+
			Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			break;

		case DMI_OPCODE_GET_MULTI_ADDRESS:
			#if DBG
			if (LanceDbg)
				DbgPrint("Multicast List: \n");
			#endif

			if (index >= Adapter->NumberOfAddresses)
			{
				ReqBlock->addr[7] = 0x55;
				break;
			}
			else
				ReqBlock->addr[7] = 0x00;

			for(i=0; i < ETH_LENGTH_OF_ADDRESS; i ++)
			{
				ReqBlock->addr[i] = (UCHAR)Adapter->MulticastAddresses[index][i];
			}

			#if DBG
			if(LanceDbg)
			{
				DbgPrint("Multicast List: %.x-%.x-%.x-%.x-%.x-%.x\n", 
						(UCHAR)AddressBuffer[index][0],
						(UCHAR)AddressBuffer[index][1],
						(UCHAR)AddressBuffer[index][2],
						(UCHAR)AddressBuffer[index][3],
						(UCHAR)AddressBuffer[index][4],
						(UCHAR)AddressBuffer[index][5]);
			}
			#endif
			break;

		case DMI_OPCODE_GET_DRIVER_VERSION:
			ReqBlock->addr[BUILD_VERS] = 0x00;	// Internal
			ReqBlock->addr[MINOR_VERS] = LANCE_DRIVER_MINOR_VERSION;
			ReqBlock->addr[MAJOR_VERS] = LANCE_DRIVER_MAJOR_VERSION;
			break;

		case DMI_OPCODE_GET_DRIVER_NAME:
			NdisMoveMemory(ReqBlock->addr,LANCE_DRIVER_NAME,sizeof(LANCE_DRIVER_NAME));
			break;

		case DMI_OPCODE_GET_TX_DUPLEX:
			switch (Adapter->FullDuplex)
			{
				case FDUP_AUI:
				case FDUP_10BASE_T:
					ReqBlock->Value = 0x02;
					break;

				default:
					ReqBlock->Value = 0x01;
					break;
			}
			break;

		case DMI_OPCODE_GET_OP_STATE:
			ReqBlock->Value = 0x5555;
			break;

		case DMI_OPCODE_GET_USAGE_STATE:
			ReqBlock->Value = 0x5555;
			break;

		case DMI_OPCODE_GET_AVAIL_STATE:
			ReqBlock->Value = 0x5555;
			break;

		case DMI_OPCODE_GET_ADMIN_STATE:
			ReqBlock->Value = 0x5555;
			break;

		case DMI_OPCODE_GET_FATAL_ERR_COUNT:
			ReqBlock->Value = 0;
			break;

		case DMI_OPCODE_GET_MAJ_ERR_COUNT:
			ReqBlock->Value = 0;
			break;

		case DMI_OPCODE_GET_WRN_ERR_COUNT:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_RESET_CNT];
			break;

		case DMI_OPCODE_GET_BOARD_FOUND:
			switch (Adapter->BoardFound)
			{
				case PCI_DEV:
					ReqBlock->Value = DMI_PCI_BOARD;
					break;

				case PLUG_PLAY_DEV:
					ReqBlock->Value = DMI_PLUG_PLAY_BOARD;
					break;

				case LOCAL_DEV:
					ReqBlock->Value = DMI_LOCAL_BOARD;
					break;				
					
				case MCA_DEV:
					ReqBlock->Value = DMI_PCI_BOARD; //MCA board is a bridge to the PCI chip
					break;       
					
				default:
					ReqBlock->Value = DMI_NO_BOARD;
					break;
			}
			break;

		case DMI_OPCODE_GET_IO_BASE_ADDR:
			ReqBlock->Value = Adapter->MappedIoBaseAddress;
			break;

		case DMI_OPCODE_GET_GET_IRQ:
			ReqBlock->Value = Adapter->LanceInterruptVector;
			break;

		case DMI_OPCODE_GET_DMA_CHANNEL:
			ReqBlock->Value = Adapter->LanceDmaChannel;
			break;

		case DMI_OPCODE_GET_CSR_VALUE:
			ReqBlock->Status = (WORD)LancePortAccess (Adapter, &ReqBlock->Value, CSR_READ);
			break;

		case DMI_OPCODE_GET_BCR_VALUE:
			ReqBlock->Status = (WORD)LancePortAccess (Adapter, &ReqBlock->Value, BCR_READ);
			break;

		case DMI_OPCODE_SET_CSR_VALUE:
			ReqBlock->Status = (WORD)LancePortAccess (Adapter, &ReqBlock->Value, CSR_WRITE);
			break;

		case DMI_OPCODE_SET_BCR_VALUE:
			ReqBlock->Status = (WORD)LancePortAccess (Adapter, &ReqBlock->Value, BCR_WRITE);
			break;

		case DMI_OPCODE_GET_CSR_NUMBER:
			ReqBlock->Value = Adapter->CsrNum;
			break;

		case DMI_OPCODE_GET_BCR_NUMBER:
			ReqBlock->Value = Adapter->BcrNum;
			break;

		case DMI_OPCODE_SET_CSR_NUMBER:
			Adapter->CsrNum = ReqBlock->Value;
			break;

		case DMI_OPCODE_SET_BCR_NUMBER:
			Adapter->BcrNum = ReqBlock->Value;
			break;

		case DMI_OPCODE_GET_CSR0_ERR: 		// Summary of CSR0 ERR bit set.
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_CSR0_ERR];
			break;

		case DMI_OPCODE_GET_CSR0_BABL:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_CSR0_BABL];
			break;

		case DMI_OPCODE_GET_CSR0_CERR:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_HEARTBEAT_FAILURE];
			break;

		case DMI_OPCODE_GET_CSR0_MISS:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_RECEIVE_NO_BUFFER];
			break;

		case DMI_OPCODE_GET_CSR0_MERR:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_CSR0_MERR];
			break;

		case DMI_OPCODE_GET_TX_DESC_ERR:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_TRANSMIT_BAD];
			break;

		case DMI_OPCODE_GET_TX_DESC_MORE:
			ReqBlock->Counter = Adapter->MediaMandatory[MM_TRANSMIT_MORE_COLLISIONS];
			break;

		case DMI_OPCODE_GET_TX_DESC_ONE:
			ReqBlock->Counter = Adapter->MediaMandatory[MM_TRANSMIT_ONE_COLLISION];
			break;

		case DMI_OPCODE_GET_TX_DESC_DEF:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_DEFERRED];
			break;

		case DMI_OPCODE_GET_TX_DESC_BUF:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_TXBUFF_ERR];
			break;

		case DMI_OPCODE_GET_TX_DESC_UFLO:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_UNDERRUN];
			break;

		case DMI_OPCODE_GET_TX_DESC_EXDEF:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_EX_DEFER];
			break;

		case DMI_OPCODE_GET_TX_DESC_LCOL:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_LATE_COLLISIONS];
			break;

		case DMI_OPCODE_GET_TX_DESC_LCAR:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_TIMES_CRS_LOST];
			break;

		case DMI_OPCODE_GET_TX_DESC_RTRY:
			ReqBlock->Counter = Adapter->MediaOptional[MO_TRANSMIT_MAX_COLLISIONS];
			break;

		case DMI_OPCODE_GET_RX_DESC_ERR:
			ReqBlock->Counter = Adapter->GeneralMandatory[GM_RECEIVE_BAD];
			break;

		case DMI_OPCODE_GET_RX_DESC_FRAM:
			ReqBlock->Counter = Adapter->MediaMandatory[MM_RECEIVE_ERROR_ALIGNMENT];
			break;

		case DMI_OPCODE_GET_RX_DESC_OFLO:
			ReqBlock->Counter = Adapter->MediaOptional[MO_RECEIVE_OVERRUN];
			break;

		case DMI_OPCODE_GET_RX_DESC_CRC:
			ReqBlock->Counter = Adapter->GeneralOptional[GO_RECEIVE_CRC-GO_ARRAY_START];
			break;

		case DMI_OPCODE_GET_RX_DESC_BUF:
			ReqBlock->Counter = Adapter->DmiSpecific[DMI_RXBUFF_ERR];
			break;

		case DMI_OPCODE_GET_LANGUAGE:
			ReqBlock->Value = MY_LANGUAGE;
			break;

		case DMI_OPCODE_GET_STATUS:
#ifdef _FAILOVER
			if (PrimaryAdapter == NULL)
				i = 0;
			else
				i = (USHORT)ActiveAdapter + 1;
			ReqBlock->addr[REDUNDANT_STATE] = (UCHAR)i;
			ReqBlock->addr[LINK_STATE] = (UCHAR) Adapter->LinkInactive;
#else
			ReqBlock->addr[REDUNDANT_STATE] = (UCHAR)0;
			ReqBlock->addr[LINK_STATE] = (UCHAR) Adapter->CableDisconnected;
#endif
			break;

		case DMI_OPCODE_GET_NET_TOP:
			if (Adapter->DeviceType == PCNET_PCI3)
				LanceGetActiveMediaInfo (Adapter);
			ReqBlock->Value = (Adapter->LineSpeed == 10)?TOPOL_10MB_ETH:TOPOL_100MB_ETH;
         break;

		case DMI_OPCODE_GET_RAM_SIZE:
			LANCE_READ_BCR (Adapter->MappedIoBaseAddress, 25, &Data);
			ReqBlock->Value = ((USHORT)Data << 8);
			break;

		default:
			ReqBlock->Status = (USHORT)DMI_REQ_FAILURE;
			break;
	}
}
STATIC
INT
LancePortAccess (PLANCE_ADAPTER Adapter, ULONG * pData, USHORT Mode)
{

USHORT	RegType		= Mode & MASK_REGTYPE;
USHORT	AccessType	= Mode & MASK_ACCESS;
INT	RetCode		= LANCE_PORT_SUCCESS;

		switch (RegType)
		{
			case CSR_REG:
				switch (AccessType)
				{
					case PORT_READ:
						LANCE_READ_CSR (Adapter->MappedIoBaseAddress, Adapter->CsrNum, pData);
						break;

					case PORT_WRITE:
						LANCE_WRITE_CSR (Adapter->MappedIoBaseAddress, Adapter->CsrNum, *pData);
						break;

					default:
						RetCode = LANCE_PORT_FAILURE;
				}
				break;

			case BCR_REG:
				switch (AccessType)
				{
					case PORT_READ:
						LANCE_READ_BCR (Adapter->MappedIoBaseAddress, Adapter->BcrNum, pData);
						break;

					case PORT_WRITE:
						LANCE_WRITE_BCR (Adapter->MappedIoBaseAddress, Adapter->BcrNum, *pData);
						break;

					default:
						RetCode = LANCE_PORT_FAILURE;
				}
				break;

			default:
				RetCode = LANCE_PORT_FAILURE;

		}

	return RetCode;
}
