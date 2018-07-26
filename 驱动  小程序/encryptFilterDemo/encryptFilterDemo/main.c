#include"encryptFilterDemo.h"
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status;
	DbgPrint("Entry DriverEntry\n");
	ExInitializeNPagedLookasideList(
		&g_ContextList,
		NULL,
		NULL,
		0,
		sizeof(PRE_2_POST_CONTEXT),
		'abc',
		0
	);
	//ע���ļ��Ļص�����
	status = FltRegisterFilter(DriverObject, &FilterRegistration, &g_filterHandle);
	if (NT_SUCCESS(status))
	{
		//��ʼ�����ļ�����
		status = FltStartFiltering(g_filterHandle);
		if (!NT_SUCCESS(status))
		{
			FltUnregisterFilter(g_filterHandle);
		}
		DbgPrint("encryptFilterDemo START\n");
	}
	//DriverObject->DriverUnload = DriverUnload;
	return status;
}