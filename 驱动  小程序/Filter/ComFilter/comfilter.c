#pragma once 
#include"comfilter.h"


//����������ֻ��32�����ڣ����Ǳ��ߵļ���
#define CCP_MAX_COM_ID 32
//�������й����豸ָ��
static PDEVICE_OBJECT s_fltobj[CCP_MAX_COM_ID] = { 0 };
//����������ʵ�豸ָ��
static PDEVICE_OBJECT s_nextobj[CCP_MAX_COM_ID] = { 0 };


//��һ���˿���Ϣ
NTSTATUS ccpAttachDevice(
	PDRIVER_OBJECT driver,
	PDEVICE_OBJECT oldobj,
	PDEVICE_OBJECT *fltobj,
	PDEVICE_OBJECT *next
)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT topdev = NULL;

	//�����豸��Ȼ���
	status = IoCreateDevice(
		driver,
		0,
		NULL,
		oldobj->DeviceType,
		0,
		FALSE,
		fltobj);
	if (status != STATUS_SUCCESS)
		return status;

	//������Ҫ��־
	if (oldobj->Flags & DO_BUFFERED_IO)
		(*fltobj)->Flags |= DO_BUFFERED_IO;
	if (oldobj->Flags & DO_DIRECT_IO)
		(*fltobj)->Flags |= DO_DIRECT_IO;
	if (oldobj->Characteristics & FILE_DEVICE_SECURE_OPEN)
		(*fltobj)->Characteristics |= FILE_DEVICE_SECURE_OPEN;
	(*fltobj)->Flags |= DO_POWER_PAGABLE;

	//��һ���豸�󶨵���һ���豸��
	topdev = IoAttachDeviceToDeviceStack(*fltobj, oldobj);
	if (topdev == NULL)
	{
		//�����ʧ���ˣ������豸�����ش���
		IoDeleteDevice(*fltobj);
		status = STATUS_UNSUCCESSFUL;
		return status;
	}
	*next = topdev;

	//��������豸�Ѿ�����
	(*fltobj)->Flags = (*fltobj)->Flags & DO_DEVICE_INITIALIZING;

	return status;
}

//��һ���˿�
PDEVICE_OBJECT ccpOpenCom(ULONG id, NTSTATUS *status)
{
	//����������Ǵ��ڵ�ID,������д���ַ�������ʽ
	UNICODE_STRING name_str;
	static WCHAR name[32] = { 0 };
	PFILE_OBJECT fileobj = NULL;
	PDEVICE_OBJECT devobj = NULL;
	//NTSTATUS status;
	//����IDת���ɴ��ڵ�����
	memset(name, 0, sizeof(WCHAR) * 32);
	RtlStringCchPrintfW(name, 32, L"\\Device\\Serial%d", id);
	RtlInitUnicodeString(&name_str, name);
	//���豸����
	*status = IoGetDeviceObjectPointer(
		&name_str,
		FILE_ALL_ACCESS,
		&fileobj, &devobj
	);
	//����򿪳ɹ��ˣ��ǵ�һ��Ҫ���ļ�����������
	//��֮����һ�䲻Ҫ����
	if (*status == STATUS_SUCCESS)
		ObReferenceObject(fileobj);
	//�����豸����
	return devobj;
}
void ccpAttachAllComs(PDRIVER_OBJECT driver)
{
	ULONG i;
	PDEVICE_OBJECT com_ob = NULL;
	NTSTATUS status;
	for (i = 0; i < CCP_MAX_COM_ID; i++)
	{
		//���object����
		com_ob = ccpOpenCom(i, &status);
		if (com_ob == NULL)
			continue;
		//������󶨣������ܰ��Ƿ�ɹ�
		ccpAttachDevice(driver, com_ob, &s_fltobj[i], &s_nextobj[i]);
	}
}

NTSTATUS ccpDispatch(PDEVICE_OBJECT device, PIRP irp)
{
	PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
	ULONG i, j;

	//����Ҫ֪�����͸����ĸ��豸���豸���һ����CCP_MAX_COM_ID
	//������ǰ��Ĵ��뱣��õģ�����s_fltobj��
	for (i = 0; i < CCP_MAX_COM_ID; i++)
	{

		if (s_fltobj[i] == device)
		{
			//���е�Դ������ȫ��ֱ�ӷŹ�
			if (irpsp->MajorFunction == IRP_MJ_POWER)
			{
				//ֱ�ӷ��ͣ�Ȼ�󷵻�˵�Ѿ���������
				PoStartNextPowerIrp(irp);
				IoSkipCurrentIrpStackLocation(irp);
				return PoCallDriver(s_nextobj[i], irp);
			}
			//��������ֻ����д����д���󣬻�û��������䳤��
			//Ȼ���ӡ
			if (irpsp->MajorFunction == IRP_MJ_WRITE)
			{
				//�����д���Ȼ�ó���
				ULONG len = irpsp->Parameters.Write.Length;
				//Ȼ���û�����
				PUCHAR buf = NULL;
				if (irp->MdlAddress != NULL)
					buf = (PUCHAR)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
				else
					buf = (PUCHAR)irp->UserBuffer;
				if (buf == NULL)
					buf = (PUCHAR)irp->AssociatedIrp.SystemBuffer;
				//��ӡ����
				for (j = 0; j < len; ++j)
				{
					DbgPrint("comcap:Send Data : %2x \r\n", buf[j]);
				}
			}
			//��Щ����ֱ���·�ִ�м��ɣ����ǲ�����ֹ���߸ı���
			IoSkipCurrentIrpStackLocation(irp);
			return IoCallDriver(s_nextobj[i], irp);
		}
	}
	//��������Ͳ��ڱ��󶨵��豸�У�����������ģ�ֱ�ӷ��ز�������
	irp->IoStatus.Information = 0;
	irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


#define DELAY_ONE_MICROSECOND (-10)
#define DELAY_ONE_MILLISECOND (DELAY_ONE_MICROSECOND*1000)
#define DELAY_ONE_SECOND  (DELAY_ONE_MILLISECOND*1000)

void ccpUnload(PDRIVER_OBJECT drv)
{
	UNREFERENCED_PARAMETER(drv);
	ULONG i;
	LARGE_INTEGER interval;
	//���Ƚ����
	for (i = 0; i < CCP_MAX_COM_ID; i++)
	{
		if (s_nextobj[i] != NULL)
			IoDetachDevice(s_nextobj[i]);
		//˯��5�롣�ȴ�����IRP�������
		interval.QuadPart = (5 * 1000 * DELAY_ONE_MILLISECOND);
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
		//ɾ����Щ�豸
		for (i = 0; i < CCP_MAX_COM_ID; i++)
		{
			if (s_fltobj[i] != NULL)
				IoDeleteDevice(s_fltobj[i]);
		}
	}
}