/*
encryptFilterDemo ���̽���

��.txt�ļ������м򵥵������ܡ�

�ڶ��ļ�֮�󣬽��н��ܣ���д�ļ�֮����м��ܡ�

ʹ��swap buffer���л������ļӽ���

*/

#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>
//ȫ�ֱ�������
extern CONST FLT_REGISTRATION FilterRegistration;
//��������
NTSTATUS InstanceSetup(PCFLT_RELATED_OBJECTS FltObjects, FLT_INSTANCE_SETUP_FLAGS Flags, DEVICE_TYPE VolumeDeviceType, FLT_FILESYSTEM_TYPE VolumeFilesystemType);
NTSTATUS
FilterUnload(
	_In_ FLT_FILTER_UNLOAD_FLAGS Flags
);
NTSTATUS
InstanceQueryTeardown(
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
VOID
CleanupVolumeContext(
	_In_ PFLT_CONTEXT Context,
	_In_ FLT_CONTEXT_TYPE ContextType
);
/********IRP_MJ_Write �Ĳ���************/
FLT_PREOP_CALLBACK_STATUS
SfPreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS
SfPostWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);
/***********IRP_MJ_Read******************************/
FLT_PREOP_CALLBACK_STATUS
SfPreRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Flt_CompletionContext_Outptr_ PVOID *CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS
SfPostRead(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_In_opt_ PVOID CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);
/*volume ������*/
typedef struct _VOLUME_CONTEXT {
	UNICODE_STRING Name;
	ULONG SectorSize;
}VOLUME_CONTEXT, *PVOLUME_CONTEXT;
/*
�ļ����������ľ������ж�ص�ʱ�� ��Ҫ�õ�
*/
extern PFLT_FILTER  g_filterHandle;
/*
����һ��ռ䣬��pre��post�н��в����Ĵ��ݡ�
*/
extern NPAGED_LOOKASIDE_LIST g_ContextList;
typedef struct _PRE_2_POST_CONTEXT {
	PVOLUME_CONTEXT VolCtx;
	PVOID newBuffer;
	PMDL  newMDL;
}PRE_2_POST_CONTEXT, *PPRE_2_POST_CONTEXT;
