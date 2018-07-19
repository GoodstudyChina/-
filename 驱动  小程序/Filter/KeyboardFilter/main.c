#pragma once 
#include<ntddk.h>
#include"keyboardfilter.h"
NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	UNREFERENCED_PARAMETER(DriverObject);

	NTSTATUS status;
	ULONG i;
	KdPrint(("Keyborad Filter is : Entrying DriverEntry\n"));
	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = keyDisaptchGeneral;
	}
	//��������дһ��Read�ַ���������Ϊ��Ҫ�Ĺ��˾��Ƕ�ȡ���İ�����Ϣ
	//�����Ķ�����Ҫ������ַ���������д
	DriverObject->MajorFunction[IRP_MJ_READ] = keyDisaptchRead;
	//��������дһ��IRP_MJ_POWER������������Ϊ���������м�Ҫ����
	//һ��PoCallDriver��һ��PoStartNextPowerIrp,�Ƚ�����
	DriverObject->MajorFunction[IRP_MJ_POWER] = keyPower;
	//������֪��ʲôʱ�����ǰ󶨹���һ���豸��ж���ˣ�����ӻ����ϱ��ε��ˣ�������ר��дһ��PNP�����弴�ã��ַ�����
	DriverObject->MajorFunction[IRP_MJ_PNP] = keyPnp;
	//ж�غ���
	DriverObject->DriverUnload = keyUnload;
	status = keyAttachDevice(DriverObject, RegistryPath);
	return status;
}