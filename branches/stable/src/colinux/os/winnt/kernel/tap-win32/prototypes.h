/*
 *  TAP-Win32 -- A kernel driver to provide virtual tap device functionality
 *               on Windows.  Originally derived from the CIPE-Win32
 *               project by Damion K. Wilson, with extensive modifications by
 *               James Yonan.
 *
 *  All source code which derives from the CIPE-Win32 project is
 *  Copyright (C) Damion K. Wilson, 2003, and is released under the
 *  GPL version 2 (see below).
 *
 *  All other source code is Copyright (C) James Yonan, 2003,
 *  and is released under the GPL version 2 (see below).
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program (see the file COPYING included with this
 *  distribution); if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TAP_PROTOTYPES_DEFINED
#define TAP_PROTOTYPES_DEFINED

NTSTATUS DriverEntry
   (
    IN PDRIVER_OBJECT p_DriverObject,
    IN PUNICODE_STRING p_RegistryPath
   );

NDIS_STATUS AdapterCreate
   (
    OUT PNDIS_STATUS p_ErrorStatus,
    OUT PUINT p_MediaIndex,
    IN PNDIS_MEDIUM p_Media,
    IN UINT p_MediaCount,
    IN NDIS_HANDLE p_AdapterHandle,
    IN NDIS_HANDLE p_ConfigurationHandle
   );

VOID AdapterHalt
   (
    IN NDIS_HANDLE p_AdapterContext
   );

VOID AdapterFreeResources
   (
    TapAdapterPointer p_Adapter
   );

NDIS_STATUS AdapterReset
   (
    OUT PBOOLEAN p_AddressingReset,
    IN NDIS_HANDLE p_AdapterContext
   );

NDIS_STATUS AdapterQuery
   (
    IN NDIS_HANDLE p_AdapterContext,
    IN NDIS_OID p_OID,
    IN PVOID p_Buffer,
    IN ULONG p_BufferLength,
    OUT PULONG p_BytesWritten,
    OUT PULONG p_BytesNeeded
   );

NDIS_STATUS AdapterModify
   (
    IN NDIS_HANDLE p_AdapterContext,
    IN NDIS_OID p_OID,
    IN PVOID p_Buffer,
    IN ULONG p_BufferLength,
    OUT PULONG p_BytesRead,
    OUT PULONG p_BytesNeeded
   );

NDIS_STATUS AdapterTransmit
   (
    IN NDIS_HANDLE p_AdapterContext,
    IN PNDIS_PACKET p_Packet,
    IN UINT p_Flags
   );

NDIS_STATUS AdapterReceive
   (
    OUT PNDIS_PACKET p_Packet,
    OUT PUINT p_Transferred,
    IN NDIS_HANDLE p_AdapterContext,
    IN NDIS_HANDLE p_ReceiveContext,
    IN UINT p_Offset,
    IN UINT p_ToTransfer
   );

NTSTATUS TapDeviceHook
   (
    IN PDEVICE_OBJECT p_DeviceObject,
    IN PIRP p_IRP
   );

NDIS_STATUS CreateTapDevice
   (
    TapAdapterPointer p_Adapter
   );

VOID DestroyTapDevice
   (
    TapAdapterPointer p_Adapter
   );

VOID TapDeviceFreeResources
   (
    TapAdapterPointer p_Adapter
    );

NTSTATUS CompleteIRP
   (
    TapAdapterPointer p_Adapter,
    IN PIRP p_IRP,
    IN TapPacketPointer p_PacketBuffer,
    IN CCHAR PriorityBoost
   );

VOID CancelIRPCallback
   (
    IN PDEVICE_OBJECT p_DeviceObject,
    IN PIRP p_IRP
   );

VOID CancelIRP
   (
    IN PDEVICE_OBJECT p_DeviceObject,
    IN PIRP p_IRP,
    BOOLEAN callback
   );

VOID FlushQueues
   (
    TapAdapterPointer p_Adapter
   );

void ResetPointToPointMode
   (
    TapAdapterPointer p_Adapter
   );

void ProcessARP
   (
    TapAdapterPointer p_Adapter,
    const PARP_PACKET src
   );

VOID SetMediaStatus
   (
    TapAdapterPointer p_Adapter,
    BOOLEAN state
   );

VOID HookDispatchFunctions();

#endif
