#pragma once 
#include"keyboardfilter.h"
typedef struct _C2P_DEV_EXT
{
	//����ṹ�Ĵ�С
	ULONG NodeSize;
	//�����豸����
	PDEVICE_OBJECT pFilterDeviceObject;
	//ͬʱ����ʱ�ı�����
	KSPIN_LOCK IoRequestsSpinLock;
	//���̼�ͬ������
	KEVENT IoInProgressEvent;
	//�󶨵��豸����
	PDEVICE_OBJECT TargetDeviceObject;
	//��ǰ�ײ��豸����
	PDEVICE_OBJECT LowerDeviceObject;
	//����ȴ���Irp  ����irp
	PIRP proxyIrp;
}C2P_DEV_EXT, *PC2P_DEV_EXT;

//�����������ʵ���ڵģ�ֻ���ĵ���û�й���
//����һ�¾Ϳ���ֱ��ʹ����
NTSTATUS ObReferenceObjectByName(
	PUNICODE_STRING ObjectName,
	ULONG Attributes,
	PACCESS_STATE AccessState,
	ACCESS_MASK DesiredAccess,
	POBJECT_TYPE ObjectType,
	KPROCESSOR_MODE AccessMode,
	PVOID ParseContext,
	PVOID *Object
);
#define KBD_DRIVER_NAME L"\\Driver\\kbdclass"
//IoDriverObjectType ʵ������һ��ȫ�ֱ���������ͷ�ļ���û��
//ֻҪ����֮��Ϳ���ʹ���ˡ����º���Ĵ������ж���õ�
extern POBJECT_TYPE *IoDriverObjectType;
//����������죬�ܴ���������KbdClass ,Ȼ���������������豸
NTSTATUS keyAttachDevice(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;
	UNICODE_STRING unNtNameString;
	PDRIVER_OBJECT KbdDriverObject = NULL;
	PDEVICE_OBJECT pTargetDeviceObject = NULL;
	PDEVICE_OBJECT pKeyDeviceObject = NULL;
	PDEVICE_OBJECT pLowerDeviceObject = NULL;
	PC2P_DEV_EXT devExt;
	KdPrint(("Attach\n"));
	//��ʼ��һ���ַ���������KdbClass����������
	RtlInitUnicodeString(&unNtNameString, KBD_DRIVER_NAME);
	

	//�����ǰ����豸��������ӣ�ֻ������򿪵�����������
	status = ObReferenceObjectByName(
		&unNtNameString,
		OBJ_CASE_INSENSITIVE,
		NULL,
		0,
		*IoDriverObjectType,
		KernelMode,
		NULL,
		(PVOID*)&KbdDriverObject
	);
	//���ʧ����ֱ�Ӿͷ���
	if (!NT_SUCCESS(status))
	{
		DbgPrint("ObReferenceObjectByName: 0x%x", status);
		return status;
	}
	else {
		//����obreferenceObjectName�ᵼ�¶�������������ü�������
		//������Ӧ�ص��ý�������ObDereferenceObject
		ObDereferenceObject(KbdDriverObject);
	}
	//�����豸�����еĵ�һ���豸
	pTargetDeviceObject = KbdDriverObject->DeviceObject;
	//��ʼ��������豸��
	while (pTargetDeviceObject)
	{
		//����һ�������豸
		status = IoCreateDevice(
			DriverObject,
			sizeof(C2P_DEV_EXT),
			NULL,
			pTargetDeviceObject->DeviceType,
			pTargetDeviceObject->Characteristics,
			FALSE,
			&pKeyDeviceObject
		);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("IoCreateDevice : 0x%x \n", status);
			return status;
		}
		//��,pLowerDeviceObjcect �ǰ�֮��õ�����һ���豸
		//Ҳ����ǰ�泣��˵����ν��ʵ�豸
		status = IoAttachDeviceToDeviceStackSafe(pKeyDeviceObject, pTargetDeviceObject,&pLowerDeviceObject);
		//�����ʧ�ܣ����������˳�
		if (!NT_SUCCESS(status))
		{
			DbgPrint("IoAttachDeviceToDeviceStachSafe : 0x%x\n", status);
			IoDeleteDevice(pKeyDeviceObject);
			pKeyDeviceObject = NULL;
			return status;
		}
		//�豸��չ������Ҫ��ϸ�����豸��չ��Ӧ��
		devExt = (PC2P_DEV_EXT)(pKeyDeviceObject->DeviceExtension);
		memset(devExt, 0, sizeof(C2P_DEV_EXT));
		devExt->NodeSize = sizeof(C2P_DEV_EXT);
		devExt->pFilterDeviceObject = pKeyDeviceObject;
		devExt->TargetDeviceObject = pTargetDeviceObject;
		devExt->LowerDeviceObject = pLowerDeviceObject;
		//�豸��չ
		pKeyDeviceObject->DeviceType = pLowerDeviceObject->DeviceType;
		pKeyDeviceObject->Characteristics = pLowerDeviceObject->Characteristics;
		pKeyDeviceObject->StackSize = pLowerDeviceObject->StackSize + 1;
		pKeyDeviceObject->Flags |= pLowerDeviceObject->Flags&(DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
		//�ƶ�����һ���豸����������
		pTargetDeviceObject = pTargetDeviceObject->NextDevice;
	}
	if (NT_SUCCESS(status))
	{
		DbgPrint("Attach Device success \n");
	}
	return status;
}




NTSTATUS keyDisaptchGeneral(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	//�����ķַ�������ֱ��Skip��Ȼ����IoCallDriver��IRP���͵���ʵ�豸���豸����
	KdPrint(("keyDisaptchGeral\n"));
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(((PC2P_DEV_EXT)(DeviceObject->DeviceExtension))->LowerDeviceObject, Irp);
}
//flags for keyboard status 
#define S_SHIFT 1
#define S_CAPS  2
#define S_NUM   4
//����һ����ǣ��������浱ǰ���̵�״̬��������3��λ���ֱ��ʾ
//Caps Lock ����Num Lock ����Shift ���Ƿ�����
static int kb_status = S_NUM;
unsigned char asciiTbl[] = {
	0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2D, 0x3D, 0x08, 0x09, //normal  
	0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6F, 0x70, 0x5B, 0x5D, 0x0D, 0x00, 0x61, 0x73,
	0x64, 0x66, 0x67, 0x68, 0x6A, 0x6B, 0x6C, 0x3B, 0x27, 0x60, 0x00, 0x5C, 0x7A, 0x78, 0x63, 0x76,
	0x62, 0x6E, 0x6D, 0x2C, 0x2E, 0x2F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
	0x32, 0x33, 0x30, 0x2E,
	0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2D, 0x3D, 0x08, 0x09, //caps  
	0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0x5B, 0x5D, 0x0D, 0x00, 0x41, 0x53,
	0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0x3B, 0x27, 0x60, 0x00, 0x5C, 0x5A, 0x58, 0x43, 0x56,
	0x42, 0x4E, 0x4D, 0x2C, 0x2E, 0x2F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
	0x32, 0x33, 0x30, 0x2E,
	0x00, 0x1B, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28, 0x29, 0x5F, 0x2B, 0x08, 0x09, //shift  
	0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50, 0x7B, 0x7D, 0x0D, 0x00, 0x41, 0x53,
	0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4C, 0x3A, 0x22, 0x7E, 0x00, 0x7C, 0x5A, 0x58, 0x43, 0x56,
	0x42, 0x4E, 0x4D, 0x3C, 0x3E, 0x3F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
	0x32, 0x33, 0x30, 0x2E,
	0x00, 0x1B, 0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A, 0x28, 0x29, 0x5F, 0x2B, 0x08, 0x09, //caps + shift  
	0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6F, 0x70, 0x7B, 0x7D, 0x0D, 0x00, 0x61, 0x73,
	0x64, 0x66, 0x67, 0x68, 0x6A, 0x6B, 0x6C, 0x3A, 0x22, 0x7E, 0x00, 0x7C, 0x7A, 0x78, 0x63, 0x76,
	0x62, 0x6E, 0x6D, 0x3C, 0x3E, 0x3F, 0x00, 0x2A, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x37, 0x38, 0x39, 0x2D, 0x34, 0x35, 0x36, 0x2B, 0x31,
	0x32, 0x33, 0x30, 0x2E
};
VOID print_keystroke(UCHAR sch)
{
	UCHAR ch = 0;
	int off = 0;


	if ((sch & 0x80) == 0)  //����ǰ���
	{
		//�����������ĸ�������ֵȿɼ��ַ�
		if ((sch < 0x47) || ((sch >= 0x47 && sch < 0x54) && (kb_status&S_NUM)))
		{
			//���յõ���һ���ַ���Caps Lock ��Num Lock ��Shift�⼸������״̬������
			if (((kb_status&S_CAPS) == S_CAPS) && ((kb_status&S_SHIFT) != S_SHIFT))
				off = 0x54;
			if (((kb_status&S_CAPS) != S_CAPS) && ((kb_status&S_SHIFT) == S_SHIFT))
				off = 0xA8;
			if (((kb_status&S_CAPS) == S_CAPS) && ((kb_status&S_SHIFT) == S_SHIFT))
				off = 0xFC;
			ch = asciiTbl[off + sch];
		}
		switch (sch)
		{
			//Caps Lock ����Num Lock �����ƣ����ǡ��������Ρ�����û����һ���ġ���������
			//������������������ñ�־��Ҳ����˵����һ�������ã��ٰ�һ�ξͲ���������
		case 0x3A:
			kb_status ^= S_CAPS;
			break;
		case 0x2A:
		case 0x36:
			kb_status |= S_SHIFT;
			break;
			//Num Lock ����
		case 0x45:
			kb_status ^= S_NUM;
			break;
		default:
			break;
		}
	}
	else {
		if (sch == 0xAA || sch == 0xB6)
			kb_status &= S_SHIFT;
	}
	if (ch >= 0x20 && ch < 0x7F)
	{
		DbgPrint("the code is : %C\n", ch);
	}
}
//����һ��IRP��ɻص�������ԭ��
NTSTATUS keyReadComplete(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID context
)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(context);
	PKEYBOARD_INPUT_DATA KeyData;
	ULONG numKeys;
	size_t i;
	PC2P_DEV_EXT devExt;
	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		//��ȡ��������ɺ�����������
		KeyData =(PKEYBOARD_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
		//��ȡ����������ĳ���
		numKeys = (Irp->IoStatus.Information) / sizeof(KEYBOARD_INPUT_DATA);
		for (i = 0; i < numKeys; i++)
		{
			DbgPrint(("%s\n", KeyData->Flags ? "UP" : "DOWN"));
			if (!KeyData->Flags)
				print_keystroke((UCHAR)KeyData->MakeCode);
		}
	}
	devExt = (PC2P_DEV_EXT)DeviceObject->DeviceExtension;
	devExt->proxyIrp = NULL;
	if (Irp->PendingReturned)
		IoMarkIrpPending(Irp);
	return Irp->IoStatus.Status;
}
NTSTATUS keyDisaptchRead(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	NTSTATUS status = STATUS_SUCCESS;
	PC2P_DEV_EXT devExt;
	PIO_STACK_LOCATION currentIrpStack;
	//�ж��Ƿ񵽴���irpջ����Ͷ�
	if (Irp->CurrentLocation == 1)
	{
		status = STATUS_INVALID_DEVICE_REQUEST;
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = 0;
		return status;
	}
	devExt = (PC2P_DEV_EXT)DeviceObject->DeviceExtension;
	devExt->proxyIrp = Irp;
	//ʣ�µ������ǵȴ����������
	currentIrpStack = IoGetCurrentIrpStackLocation(Irp);
	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, keyReadComplete, DeviceObject, TRUE, TRUE, TRUE);
	return IoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS keyPower(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp
)
{
	PC2P_DEV_EXT devExt;
	devExt = (PC2P_DEV_EXT)(DeviceObject->DeviceExtension);
	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return PoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS keyPnp(IN PDEVICE_OBJECT DevieObject, IN PIRP Irp)
{
	PC2P_DEV_EXT devExt;
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpStack;

	devExt = (PC2P_DEV_EXT)DevieObject->DeviceExtension;
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	switch (irpStack->MinorFunction)
	{
		//���̰ε�֮��
	case IRP_MN_REMOVE_DEVICE:
		KdPrint(("IRP_MN_REMOVE_DEVICE\n"));
		//���Ȱ������·���ȥ
		IoSkipCurrentIrpStackLocation(Irp);
		IoCallDriver(devExt->LowerDeviceObject, Irp);
		//Ȼ������
		IoDetachDevice(devExt->LowerDeviceObject);
		//ɾ�������Լ����ɵ������豸
		IoDeleteDevice(DevieObject);
		status = STATUS_SUCCESS;
		break;
	default:
		//������IRP��ȫ��ֱ���·�
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(devExt->LowerDeviceObject, Irp);
		break;
	}
	return status;
}
BOOLEAN CancelIrp(PIRP pIrp)
{
	if (pIrp == NULL)
	{
		DbgPrint("ȡ��irp����.../n");
		return FALSE;
	}
	if (pIrp->Cancel || pIrp->CancelRoutine == NULL)
	{
		DbgPrint("ȡ��irp����.../n");
		return FALSE;
	}
	if (FALSE == IoCancelIrp(pIrp))
	{
		DbgPrint("IoCancelIrp to irp����.../n");
		return FALSE;
	}

	//ȡ�����������Ϊ��  
	IoSetCancelRoutine(pIrp, NULL);
	return TRUE;
}

VOID  keyUnload(
	IN PDRIVER_OBJECT DriverObject
)
{
	UNREFERENCED_PARAMETER(DriverObject);
	KdPrint(("DriverEntry unloading ....\n"));
	PDEVICE_OBJECT DeviceObject;
	//PDEVICE_OBJECT OldDeviceObject;
	PRKTHREAD CurrentThread;
	CurrentThread = KeGetCurrentThread();
	//�ѵ�ǰ�߳�����Ϊ��ʵʱģʽ���Ա����������о�����Ӱ����������
	KeSetPriorityThread(CurrentThread, LOW_REALTIME_PRIORITY);
	//���������豸��һ�ɽ����
	DeviceObject = DriverObject->DeviceObject;

	while (DeviceObject)
	{
		//����󶨲�ɾ�����е��豸
		//c2pDetach(DeviceObject);
		PC2P_DEV_EXT devExt;
		devExt = (PC2P_DEV_EXT)DeviceObject->DeviceExtension;
		if (devExt->LowerDeviceObject != NULL)
			IoDetachDevice(devExt->LowerDeviceObject);
		devExt->LowerDeviceObject = NULL;
		if (devExt->pFilterDeviceObject != NULL)
			IoDeleteDevice(devExt->pFilterDeviceObject);
		devExt->pFilterDeviceObject = NULL;
		if (devExt->proxyIrp != NULL)
		{
			//ȡ����ǰIRP
			CancelIrp(devExt->proxyIrp);
		}
		DeviceObject = DeviceObject->NextDevice;
	}
	KdPrint(("DriverEntry unlLoad OK!\n"));
	return;
}