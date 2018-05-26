#pragma once

#include<ntddk.h>
#include<ntdef.h>
#include<wdm.h>


typedef struct _LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT TlsIndex;
	union {
		LIST_ENTRY HashLinks;
		struct
		{
			PVOID SectionPointer;
			ULONG CheckSum;
		}d;
	}s;
	union {
		struct
		{
			ULONG TimeDateStamp;
		}q;
		struct
		{
			PVOID LoadedImports;
		}w;
	}e;
	PVOID EntryPointActivationContext;
	PVOID PatchInformation;
}LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

PVOID obHandle;	//����һ��void*���͵ı�������������ΪObRegisterCallbacks�����ĵ�2��������

VOID DriverUnload(IN PDRIVER_OBJECT pDriverObj)
{
	UNREFERENCED_PARAMETER(pDriverObj);

	if (NULL != obHandle)
	{
		ObUnRegisterCallbacks(obHandle); //ж�ػص�����
	}

	KdPrint(("Unloading .....\n"));
}

OB_PREOP_CALLBACK_STATUS
preCall(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION pOperationInformation)
{
	UNREFERENCED_PARAMETER(RegistrationContext);
	UNREFERENCED_PARAMETER(pOperationInformation);

	KdPrint(("a new process by create \n "));////������ ���´����Ľ��� ���в�����

	return OB_PREOP_SUCCESS;
}

NTSTATUS ProtectProcess()
{
	NTSTATUS re = STATUS_SUCCESS;
	OB_CALLBACK_REGISTRATION obReg;
	OB_OPERATION_REGISTRATION opReg;


	memset(&obReg, 0, sizeof(obReg));
	obReg.Version = ObGetFilterVersion();
	obReg.OperationRegistrationCount = 1;
	obReg.RegistrationContext = NULL;
	RtlInitUnicodeString(&obReg.Altitude, L"321000");

	memset(&opReg, 0, sizeof(opReg)); //��ʼ���ṹ�����

									  //���� ��ע������ṹ��ĳ�Ա�ֶε�����
	opReg.ObjectType = PsProcessType;
	opReg.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;

	opReg.PreOperation = (POB_PRE_OPERATION_CALLBACK)&preCall; //������ע��һ���ص�����ָ��

	obReg.OperationRegistration = &opReg; //ע����һ�����

	re = ObRegisterCallbacks(&obReg, &obHandle); //������ע��ص�����
	if (!NT_SUCCESS(re))
	{
		KdPrint(("ObRegistreCallbacks failed \n"));
	}
	return re;
}

NTSTATUS
DriverEntry(IN PDRIVER_OBJECT pDriverObj, IN PUNICODE_STRING pRegistryString)
{
	NTSTATUS re = STATUS_SUCCESS;
	
	UNREFERENCED_PARAMETER(pDriverObj);
	UNREFERENCED_PARAMETER(pRegistryString);
	KdPrint(("My Driver install success \r\n"));

	pDriverObj->DriverUnload = DriverUnload;

	//Protect by process  name

	//�ƹ�MmVerifyCallBackFunction
	PLDR_DATA_TABLE_ENTRY ldr;
	ldr = (PLDR_DATA_TABLE_ENTRY)pDriverObj->DriverSection;
	ldr->Flags |= 0x20;
	re = ProtectProcess();
	return re;
}