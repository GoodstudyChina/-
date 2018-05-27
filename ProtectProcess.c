#pragma once

#include<ntddk.h>
#include<ntdef.h>
#include<wdm.h>
//#include<ntifs.h>


// �����ڱ��ļ���ʹ���ˣ��ú꣬�����ļ��в���ʹ�øú�
#define PROCESS_TERMINATE                  (0x0001)  
#define PROCESS_CREATE_THREAD              (0x0002)  
#define PROCESS_SET_SESSIONID              (0x0004)  
#define PROCESS_VM_OPERATION               (0x0008)  
#define PROCESS_VM_READ                    (0x0010)  
#define PROCESS_VM_WRITE                   (0x0020)  
#define PROCESS_DUP_HANDLE                 (0x0040)  
#define PROCESS_CREATE_PROCESS             (0x0080)  
#define PROCESS_SET_QUOTA                  (0x0100)  
#define PROCESS_SET_INFORMATION            (0x0200)  
#define PROCESS_QUERY_INFORMATION          (0x0400)  
#define PROCESS_SUSPEND_RESUME             (0x0800)  
#define PROCESS_QUERY_LIMITED_INFORMATION  (0x1000)  



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




//��ǰ���� 
NTKERNELAPI
UCHAR * PsGetProcessImageFileName(__in PEPROCESS Process);

NTKERNELAPI NTSTATUS  PsLookupProcessByProcessId(
	HANDLE    ProcessId,
	PEPROCESS *Process
);
char* GetProcessImageNameByProcessID(ULONG ulProcessID)
{
	NTSTATUS  Status;
	PEPROCESS  EProcess = NULL;


	Status = PsLookupProcessByProcessId((HANDLE)ulProcessID, &EProcess);    //EPROCESS ���������������Ҫ�ڵ���ǰ ��������

																			//ͨ�������ȡEProcess
	if (!NT_SUCCESS(Status))
	{
		return FALSE;
	}
	ObDereferenceObject(EProcess);
	//ͨ��EProcess��ý�������
	return (char*)PsGetProcessImageFileName(EProcess);

}


OB_PREOP_CALLBACK_STATUS
preCall(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION pOperationInformation)
{
	UNREFERENCED_PARAMETER(RegistrationContext);
	UNREFERENCED_PARAMETER(pOperationInformation);

	//KdPrint(("a new process by create \n "));////������ ���´����Ľ��� ���в�����
	HANDLE pid = PsGetProcessId((PEPROCESS)pOperationInformation->Object);    ///Ŀ����̵�ID
	char szProcName[16] = { 0 };
	strcpy(szProcName, GetProcessImageNameByProcessID((ULONG)pid));

	//����ʹ��ntopenprocess �������򿪽��̵ľ��
	if (!_stricmp(szProcName, "calc.exe"))
	//if(1)
	{
		if (pOperationInformation->Operation == OB_OPERATION_HANDLE_CREATE)
		{
			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_TERMINATE) == PROCESS_TERMINATE)
			{
				//Terminate the process, such as by calling the user-mode TerminateProcess routine..
				KdPrint(("--------%s\r\n", szProcName));
				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_TERMINATE;
			}
			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_OPERATION) == PROCESS_VM_OPERATION)
			{
				//Modify the address space of the process, such as by calling the user-mode WriteProcessMemory and VirtualProtectEx routines.
				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_OPERATION;
			}
			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_READ) == PROCESS_VM_READ)
			{
				//Read to the address space of the process, such as by calling the user-mode ReadProcessMemory routine.
				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_READ;
			}
			if ((pOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess & PROCESS_VM_WRITE) == PROCESS_VM_WRITE)
			{
				//Write to the address space of the process, such as by calling the user-mode WriteProcessMemory routine.
				pOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= ~PROCESS_VM_WRITE;
			}
		}
	}
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