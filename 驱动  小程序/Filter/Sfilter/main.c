#pragma once
#include<ntifs.h>
#include<ntddk.h>
#include<Ntdddisk.h>
#include"sfFastio.h"
//
//  Holds pointer to the device object that represents this driver and is used
//  by external programs to access this driver.  This is also known as the
//  "control device object".
//
PDEVICE_OBJECT gSFilterControlDeviceObject;

//
//  Holds pointer to the driver object for this driver
//
PDRIVER_OBJECT gSFilterDriverObject = NULL;

//
//  Buffer size for local names on the stack
//
#define MAX_DEVNAME_LENGTH 64
//
//  This lock is used to synchronize our attaching to a given device object.
//  This lock fixes a race condition where we could accidently attach to the
//  same device object more then once.  This race condition only occurs if
//  a volume is being mounted at the same time as this filter is being loaded.
//  This problem will never occur if this filter is loaded at boot time before
//  any file systems are loaded.
//
//  This lock is used to atomically test if we are already attached to a given
//  device object and if not, do the attach.
//

FAST_MUTEX gSfilterAttachLock;


//
//  TAG identifying memory SFilter allocates
//

#define SFLT_POOL_TAG   'tlFS'


//
//  Macro to test if this is my device object
//

#define IS_MY_DEVICE_OBJECT(_devObj) \
    (((_devObj) != NULL) && \
     ((_devObj)->DriverObject == gSFilterDriverObject) && \
      ((_devObj)->DeviceExtension != NULL))// && \
	 // ((*(ULONG *)(_devObj)->DeviceExtension) == SFLT_POOL_TAG))


 //�ļ�����ϵͳ�������豸��չ
typedef struct _SFILTER_DEVICE_EXTENSION {
	//�󶨵��ļ�ϵͳ�豸����ʵ�豸��
	PDEVICE_OBJECT AttachedToDeviceObject;
	//���ļ�ϵͳ�豸��ص���ʵ�豸�����̣�,����ڰ�ʱʹ��
	PDEVICE_OBJECT StorageStackDeviceObject;
	//�������һ������ô����������̾���;�������ǰ󶨵Ŀ����豸��
	UNICODE_STRING DeviceName;
	//�������������ַ����Ļ�����
	WCHAR DeviceNameBuffer[MAX_DEVNAME_LENGTH];
} SFILTER_DEVICE_EXTENSION, *PSFILTER_DEVICE_EXTENSION;
//�����ļ�ϵͳ�����̣�CD_ROM���������ļ�ϵͳ
#define IS_DESIRED_DEVICE_TYPE(_type) \
    (((_type) == FILE_DEVICE_DISK_FILE_SYSTEM) || \
     ((_type) == FILE_DEVICE_CD_ROM_FILE_SYSTEM) || \
     ((_type) == FILE_DEVICE_NETWORK_FILE_SYSTEM))




NTSTATUS SfPassThrough(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	PSFILTER_DEVICE_EXTENSION pevExt;
	pevExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(pevExt->AttachedToDeviceObject,Irp);
}

NTSTATUS sfAttachDeviceToDeviceStack(
	IN PDEVICE_OBJECT SourceDevice,
	IN PDEVICE_OBJECT TargetDevice,
	IN OUT PDEVICE_OBJECT *AttachedToDeviceObject
)
{
	//���Դ��룬������������Ƿ���������ڿ�ҳ������
	PAGED_CODE();
	UNICODE_STRING usName;
	NTSTATUS status;
	RtlInitUnicodeString(&usName, L"IoAttachDeviceToDeviceStackSafe\n");
	status = IoAttachDeviceToDeviceStackSafe(SourceDevice, TargetDevice, AttachedToDeviceObject);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoAttachDeviceToDeviceStackSafe : 0x%x\n");
	}
	return status;
}



VOID SfGetObjectName(
	IN PVOID Object,
	IN OUT PUNICODE_STRING Name
)
{
	NTSTATUS status;
	CHAR nibuf[512];
	POBJECT_NAME_INFORMATION  nameInfo = (POBJECT_NAME_INFORMATION)nibuf;
	ULONG retLength;
	status = ObQueryNameString(Object, nameInfo, sizeof(nibuf), &retLength);
	Name->Length = 0;
	if (NT_SUCCESS(status))
	{
		RtlCopyUnicodeString(Name, &nameInfo->Name);
	}
	else {
		DbgPrint("ObQueryNameString : 0x%x \n", status);
	}
}
NTSTATUS
SfAttachToFileSystemDeivce(
	IN PDEVICE_OBJECT DeviceObject,
	IN PUNICODE_STRING DeviceName
)
{
	PDEVICE_OBJECT newDeviceObject;
	PSFILTER_DEVICE_EXTENSION devExt;
	UNICODE_STRING fsrecName;
	NTSTATUS status;
	UNICODE_STRING fsName;
	WCHAR tempNameBuffer[MAX_DEVNAME_LENGTH];
	PAGED_CODE();
	//����豸����
	if (!IS_DESIRED_DEVICE_TYPE(DeviceObject->DeviceType))
	{
		return STATUS_SUCCESS;
	}
	RtlInitEmptyUnicodeString(&fsName, tempNameBuffer, sizeof(tempNameBuffer));
	RtlInitUnicodeString(&fsrecName, L"\\FileSystem\\Fs_Rec");
	SfGetObjectName(DeviceObject->DriverObject, &fsName);
	//�����ļ�ʶ�����İ�
	if (RtlCompareUnicodeString(&fsName, &fsrecName, TRUE) == 0)
	{
		return STATUS_SUCCESS;
	}
	//�����µ��豸��׼����Ŀ���豸
	status = IoCreateDevice(
		gSFilterDriverObject,
		sizeof(SFILTER_DEVICE_EXTENSION),
		NULL,
		DeviceObject->DeviceType,
		0,
		FALSE,
		&newDeviceObject
	);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateDevice : 0x%x \n", status);
	}
	//���Ƹ��ֱ�־
	if (FlagOn(DeviceObject->Flags, DO_BUFFERED_IO))
		SetFlag(newDeviceObject->Flags, DO_BUFFERED_IO);
	if (FlagOn(DeviceObject->Flags, DO_DIRECT_IO))
		SetFlag(newDeviceObject->Flags, DO_DIRECT_IO);
	if (FlagOn(DeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN))
		SetFlag(newDeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN);

	devExt = (PSFILTER_DEVICE_EXTENSION)newDeviceObject->DeviceExtension;
	//ʹ����һ���ṩ�ĺ������а�
	status = sfAttachDeviceToDeviceStack(newDeviceObject, DeviceObject, &devExt->AttachedToDeviceObject);
	RtlInitEmptyUnicodeString(&devExt->DeviceName, devExt->DeviceNameBuffer, sizeof(devExt->DeviceNameBuffer));
	RtlCopyUnicodeString(&devExt->DeviceName, DeviceName);
	ClearFlag(newDeviceObject->Flags, DO_DEVICE_INITIALIZING);
	//�����ǲ�ͬ�汾�ļ�������ơ�������Ŀ�����ϵͳ�İ汾����0x501ʱ
	//windows �ں�һ����EnumerateDeviceObjectList�Ⱥ�������ʱ�����ö��
	//���еľ����󶨡����������Ŀ�����ϵͳ�����С����ô��Щ�������������ڣ������޷����Ѿ����صľ�
	//WINVER >= 0x0501 windows xp
	//status = SfEnumerateFileSystemVolumes(DevieObject,&faName);
	return status;
}


VOID
SfDetachFromFileSystemDevice(
	IN PDEVICE_OBJECT DeviceObject
)
{
	PDEVICE_OBJECT ourAttachedDevice;
	PSFILTER_DEVICE_EXTENSION devExt;

	PAGED_CODE();

	//DeviceObject->AttachedDevice
	ourAttachedDevice = DeviceObject->AttachedDevice;
	while (NULL!=ourAttachedDevice)
	{
		if (IS_MY_DEVICE_OBJECT(ourAttachedDevice))
		{
			devExt = ourAttachedDevice->DeviceExtension;
			IoDetachDevice(DeviceObject);
			IoDeleteDevice(DeviceObject);
			return;
		}
		DeviceObject = ourAttachedDevice;
		ourAttachedDevice = DeviceObject->AttachedDevice;
	}
}
//ע��һ���ص�����
VOID SfFsNotification(
	IN PDEVICE_OBJECT DeviceObject,
	IN BOOLEAN FsActive
)
{


	//FsActive ΪTRUE ���ʾ�ļ�ϵͳ�ļ�����ΪFALSE���ʾ�ļ�ϵͳ��ж��
	PAGED_CODE();
	UNICODE_STRING name;
	WCHAR nameBuffer[MAX_DEVNAME_LENGTH];
	RtlInitEmptyUnicodeString(&name, nameBuffer, sizeof(nameBuffer));
	SfGetObjectName(DeviceObject, &name);
	DbgPrint("NAME : %wZ\n", name);
#if DBG
	_asm int 3;
#endif // DBG
	//������������⡣������ļ�ϵͳ�����ô���ļ�ϵͳ�Ŀ����豸
	//�����ע���������󶨡�
	if (FsActive)
	{
		SfAttachToFileSystemDeivce(DeviceObject, &name);
	}
	else {
		SfDetachFromFileSystemDevice(DeviceObject);
	}
}

//ע��IRP��ɣ��ص�����
NTSTATUS SfFsControlCompletion(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context
)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);
	KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}
//�ļ�ϵͳʶ�����Ӵ���
NTSTATUS SfFsControlLoadFileSystem(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	PSFILTER_DEVICE_EXTENSION devExt;
	NTSTATUS status;
	KEVENT waitEvent;
	KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
	IoCopyCurrentIrpStackLocationToNext(Irp);
	
	IoSetCompletionRoutine(
		Irp,
		SfFsControlCompletion,
		&waitEvent,
		TRUE,
		TRUE,
		TRUE
		);
	devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	status = IoCallDriver(devExt->AttachedToDeviceObject, Irp);
	if (status == STATUS_PENDING)
		status = KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, NULL);
	//ɾ��ϵͳʶ�����Ĺ���
	IoDetachDevice(DeviceObject);
	IoDeleteDevice(DeviceObject);
	status = Irp->IoStatus.Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
NTSTATUS
SfIsShadowCopyVolume(
	IN PDEVICE_OBJECT StorageStackDeviceObject,
	OUT PBOOLEAN IsShadowCopy
)
{
	PAGED_CODE();
	*IsShadowCopy = FALSE;
	PIRP irp;
	KEVENT event;
	IO_STATUS_BLOCK iosb;
	NTSTATUS status;

	if (FILE_DEVICE_VIRTUAL_DISK != StorageStackDeviceObject->DeviceType) {

		return STATUS_SUCCESS;
	}
	
	KeInitializeEvent(&event, NotificationEvent, FALSE);
	irp = IoBuildDeviceIoControlRequest(IOCTL_DISK_IS_WRITABLE,
		StorageStackDeviceObject,
		NULL,
		0,
		NULL,
		0,
		FALSE,
		&event,
		&iosb);
	
	if (irp == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	}
	status = IoCallDriver(StorageStackDeviceObject, irp);
	if (status == STATUS_PENDING) {

	(VOID)KeWaitForSingleObject(&event,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	status = iosb.Status;
	}
	if (STATUS_MEDIA_WRITE_PROTECTED == status) {
		*IsShadowCopy = TRUE;
		status = STATUS_SUCCESS;
	}
	return status;
}
BOOLEAN
SfIsAttachedToDeviceWXPAndLater(
	PDEVICE_OBJECT DeviceObject,
	PDEVICE_OBJECT *AttachedDeviceObject OPTIONAL
)
{
	PDEVICE_OBJECT currentDevObj;
	PDEVICE_OBJECT nextDevObj;
	PAGED_CODE();
	//
	//  Get the device object at the TOP of the attachment chain
	//
	//
	//RtlInitUnicodeString(&functionName, L"IoGetAttachedDeviceReference");
	//gSfDynamicFunctions.GetAttachedDeviceReference = MmGetSystemRoutineAddress(&functionName);
	//PSF_GET_ATTACHED_DEVICE_REFERENCE 
	//ASSERT(NULL != gSfDynamicFunctions.GetAttachedDeviceReference);
	currentDevObj = IoGetAttachedDeviceReference(DeviceObject);
	//
	//  Scan down the list to find our device object.
	//
	do {

		if (IS_MY_DEVICE_OBJECT(currentDevObj)) {
			//
			//  We have found that we are already attached.  If we are
			//  returning the device object, leave it referenced else remove
			//  the reference.
			//
			if (ARGUMENT_PRESENT(AttachedDeviceObject)) {
				*AttachedDeviceObject = currentDevObj;
			}
			else {
				ObDereferenceObject(currentDevObj);
			}
			return TRUE;
		}
		//
		//  Get the next attached object.  This puts a reference on 
		//  the device object.
		//
		//RtlInitUnicodeString( &functionName, L"IoGetLowerDeviceObject" );
		//gSfDynamicFunctions.GetLowerDeviceObject = MmGetSystemRoutineAddress(&functionName);
		//ASSERT(NULL != gSfDynamicFunctions.GetLowerDeviceObject);
		nextDevObj = IoGetLowerDeviceObject(currentDevObj);
		//
		//  Dereference our current device object, before
		//  moving to the next one.
		//
		ObDereferenceObject(currentDevObj);
		currentDevObj = nextDevObj;
	} while (NULL != currentDevObj);

	if (ARGUMENT_PRESENT(AttachedDeviceObject)) {

		*AttachedDeviceObject = NULL;
	}
	return FALSE;
}
NTSTATUS SfAttachToMountedDevice(
	IN  PDEVICE_OBJECT DeviceObject,
	IN PDEVICE_OBJECT SFilterDeviceObject
)
{
	PSFILTER_DEVICE_EXTENSION newDevExt = (PSFILTER_DEVICE_EXTENSION)SFilterDeviceObject->DeviceExtension;
	NTSTATUS status;
	ULONG i;
	PAGED_CODE();
	//�豸��ǵĸ���
	if (FlagOn(DeviceObject->Flags, DO_BUFFERED_IO))
		SetFlag(SFilterDeviceObject->Flags, DO_BUFFERED_IO);
	if (FlagOn(DeviceObject->Flags, DO_DIRECT_IO))
		SetFlag(SFilterDeviceObject->Flags, DO_DIRECT_IO);
	//ѭ�����԰󶨣�
	for (i = 0; i < 8; i++)
	{
		LARGE_INTEGER interval;
		status = sfAttachDeviceToDeviceStack(SFilterDeviceObject, DeviceObject, &newDevExt->AttachedToDeviceObject);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("sfAttacheDeviceToDeviceStack faile : 0x%x\n",status);
			return status;
		}
		//�ӳ�500ms���ټ���
		interval.QuadPart = (500*1000);
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}
	return status;
}
//�󶨾��ʵ��
NTSTATUS SfFsControlMountVolumeComplete(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PDEVICE_OBJECT NewDeviceObject
)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	PVPB vpb;
	PSFILTER_DEVICE_EXTENSION newDevExt;
	PIO_STACK_LOCATION irpSp;
	PDEVICE_OBJECT attachedDeviceObject;
	NTSTATUS status;
	//��ȡVpb
	newDevExt = (PSFILTER_DEVICE_EXTENSION)NewDeviceObject->DeviceExtension;
	vpb = newDevExt->StorageStackDeviceObject->Vpb;
	irpSp = IoGetCurrentIrpStackLocation(Irp);
	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		//���һ��������
		ExAcquireFastMutex(&gSfilterAttachLock);
		//�ж��Ƿ�󶨹�
		
		if (!SfIsAttachedToDeviceWXPAndLater(vpb->DeviceObject, &attachedDeviceObject))
		{
			//��
			status = SfAttachToMountedDevice(vpb->DeviceObject,NewDeviceObject);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("SfAttachToMountedDevice fail : 0x%x\n", status);
			}
		}
		else {
			//�Ѿ��󶨹���

			IoDeleteDevice(NewDeviceObject);
			ObDereferenceObject((PVOID)attachedDeviceObject);
		}
		ExReleaseFastMutex(&gSfilterAttachLock);
	}
	else {
		IoDeleteDevice(NewDeviceObject);
	}
	//�������
	status = Irp->IoStatus.Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
//���ؾ�
NTSTATUS SfFsControlMountVolume(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
{
	PAGED_CODE();
	PSFILTER_DEVICE_EXTENSION devExt = DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PDEVICE_OBJECT newDeviceObject;
	PDEVICE_OBJECT storageStackDeviceObject;
	NTSTATUS status;
	BOOLEAN isShadowCopyVoulume;
	PSFILTER_DEVICE_EXTENSION newDevExt;
	//����Vpb->RealDevice
	storageStackDeviceObject = irpSp->Parameters.MountVolume.Vpb->RealDevice;
	//�ж��Ƿ��Ǿ�Ӱ
	status = SfIsShadowCopyVolume(storageStackDeviceObject, &isShadowCopyVoulume);
	//���󶨾�Ӱ
	if (NT_SUCCESS(status) && isShadowCopyVoulume)
	{
		IoSkipCurrentIrpStackLocation(Irp);
		return IoCallDriver(devExt->AttachedToDeviceObject, Irp);
	}
	//Ԥ�����ɹ����豸
	status = IoCreateDevice(
		gSFilterDriverObject,
		sizeof(SFILTER_DEVICE_EXTENSION),
		NULL,
		DeviceObject->DeviceType,
		0,
		FALSE,
		&newDeviceObject
	);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateDevie faile: 0x%x \n", status);
		Irp->IoStatus.Information = 0;
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}
	newDevExt = (PSFILTER_DEVICE_EXTENSION)newDeviceObject->DeviceExtension;
	newDevExt->StorageStackDeviceObject = storageStackDeviceObject;
	RtlInitEmptyUnicodeString(&newDevExt->DeviceName, newDevExt->DeviceNameBuffer, sizeof(newDevExt->DeviceNameBuffer));
	SfGetObjectName(storageStackDeviceObject, &newDevExt->DeviceName);

	if (1)
	{
		KEVENT waitEvent;
		KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(
			Irp,SfFsControlCompletion,
			&waitEvent,
			TRUE, TRUE, TRUE);
		status = IoCallDriver(devExt->AttachedToDeviceObject, Irp);
		if (STATUS_PENDING == status)
			status = KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, NULL);
	}
	//������������ɣ����ú����󶨾�
	status = SfFsControlMountVolumeComplete(DeviceObject, Irp, newDeviceObject);
	return status;
}
//
NTSTATUS SfFsControl(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	//NTSTATUS status;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	PAGED_CODE();
	switch (irpSp->MinorFunction)
	{
		case IRP_MN_MOUNT_VOLUME:  //˵��һ��������
			return SfFsControlMountVolume(DeviceObject, Irp);
		case IRP_MN_LOAD_FILE_SYSTEM: 
			SfFsControlLoadFileSystem(DeviceObject, Irp);
			break;
		case IRP_MN_USER_FS_REQUEST:
		{
			switch (irpSp->Parameters.FileSystemControl.FsControlCode)
			{
				case FSCTL_DISMOUNT_VOLUME:
				{
					//PSFILTER_DEVICE_EXTENSION devExt = (PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension;  //ִ��ж�ز���
					break;
				}
			}
			break;
		}
	}
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(((PSFILTER_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->AttachedToDeviceObject, Irp);
}

NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;
	ULONG i;
	//����һ��Unicode�ַ���
	UNICODE_STRING nameString;
	RtlInitUnicodeString(&nameString, L"\\FileSystem\\Filters\\SFilter");
	//���ɿ����豸
	status = IoCreateDevice(
		DriverObject,
		0,
		&nameString,
		FILE_DEVICE_DISK_FILE_SYSTEM,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&gSFilterControlDeviceObject
		);
	if (status == STATUS_OBJECT_PATH_NOT_FOUND)
	{
		//������ΪһЩ�Ͱ汾�Ĳ���ϵͳû��\FileSystem\Filters\���Ŀ¼
		//���û�У���ı�λ�ã����ɵ�\FileSystem��
		RtlInitUnicodeString(&nameString, L"\\FileSystem\\SFilterCDO");
		status = IoCreateDevice(
			DriverObject,
			0, &nameString,
			FILE_DEVICE_DISK_FILE_SYSTEM,
			FILE_DEVICE_SECURE_OPEN,
			FALSE,
			&gSFilterControlDeviceObject
		);
	}
	if (!NT_SUCCESS(status))
	{
		DbgPrint("IoCreateDevice : 0x%x", status);
	}

	//-----------------��ͨ�ַ�����-------------------
	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		DriverObject->MajorFunction[i] = SfPassThrough;


	DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] = SfFsControl;

	//--------------------------------------���ٷַ�����----------------------
	PFAST_IO_DISPATCH fastIoDispatch;
	fastIoDispatch = ExAllocatePoolWithTag(
		NonPagedPool, sizeof(FAST_IO_DISPATCH), 'g'
	);
	if (!fastIoDispatch)
	{
		//����ʧ�ܵ������ɾ�������ɵĿ����豸
		IoDeleteDevice(gSFilterControlDeviceObject);
		DbgPrint("ExAllocatePoolWithTag  error \n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	//�ڴ�����
	RtlZeroMemory(fastIoDispatch, sizeof(FAST_IO_DISPATCH));
	fastIoDispatch->SizeOfFastIoDispatch = sizeof(FAST_IO_DISPATCH);
	fastIoDispatch->FastIoCheckIfPossible = SfFastIoCheckIfPossible;
	fastIoDispatch->FastIoRead = SfFastIoRead;
	fastIoDispatch->FastIoWrite = SfFastIoWrite;
	fastIoDispatch->FastIoQueryBasicInfo = SfFastIoQueryBasicInfo;
	fastIoDispatch->FastIoQueryStandardInfo = SfFastIoQueryStandardInfo;
	fastIoDispatch->FastIoLock = SfFastIoLock;
	fastIoDispatch->FastIoUnlockSingle = SfFastIoUnlockSingle;
	fastIoDispatch->FastIoUnlockAll = SfFastIoUnlockAll;
	fastIoDispatch->FastIoUnlockAllByKey = SfFastIoUnlockAllByKey;
	fastIoDispatch->FastIoDeviceControl = SfFastIoDeviceControl;
	fastIoDispatch->FastIoDetachDevice = SfFastIoDetachDevice;
	fastIoDispatch->FastIoQueryNetworkOpenInfo = SfFastIoQueryNetworkOpenInfo;
	fastIoDispatch->MdlRead = SfFastIoMdlRead;
	fastIoDispatch->MdlReadComplete = SfFastIoMdlReadComplete;
	fastIoDispatch->PrepareMdlWrite = SfFastIoPrepareMdlWrite;
	fastIoDispatch->MdlWriteComplete = SfFastIoMdlWriteComplete;
	fastIoDispatch->FastIoReadCompressed = SfFastIoReadCompressed;
	fastIoDispatch->FastIoWriteCompressed = SfFastIoWriteCompressed;
	fastIoDispatch->MdlReadCompleteCompressed = SfFastIoMdlReadCompleteCompressed;
	fastIoDispatch->MdlWriteCompleteCompressed = SfFastIoMdlWriteCompleteCompressed;
	fastIoDispatch->FastIoQueryOpen = SfFastIoQueryOpen;
	//����ƶ�DriverObject
	DriverObject->FastIoDispatch = fastIoDispatch;
	//-----------------------------------------------------------------------------------


	//--------------ע���ļ��䶯�ص�����---��windows xp֮�󣬻�����Ѿ����ڵ��ļ�ϵͳ--------------
	status = IoRegisterFsRegistrationChange(DriverObject, SfFsNotification);
	if (!NT_SUCCESS(status))
	{
		//��һʧ���ˣ�ǰ������FastIo�ַ�������û���ˣ�ֱ���ͷŵ�
		DbgPrint("IoRegisterFsRegistrationChange : 0x%x\n", status);
		DriverObject->FastIoDispatch = NULL;
		ExFreePool(fastIoDispatch);
		//ǰ�����ɵĿ����豸Ҳ�������ˣ�ɾ���Ƴ�
		IoDeleteDevice(gSFilterControlDeviceObject);
		return status;
	}
	return status;
}