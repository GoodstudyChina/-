#include<ntifs.h>

/*
/ͨ���Ľ��̵�ID�����ؽ��̵�����
/���ø�ʽһ��Ϊ��
/
UNICODE_STRING ParentName;
ParentName.Length = 0;
ParentName.MaximumLength = 0;
ParentName.Buffer = NULL;

if (GetProcessImagePath(&ParentName, CreateInfo->ParentProcessId) == STATUS_BUFFER_OVERFLOW)
{
ParentName.Buffer = ExAllocatePoolWithTag(NonPagedPool, ParentName.Length,'me');
ParentName.MaximumLength = ParentName.Length;
GetProcessImagePath(&ParentName, CreateInfo->ParentProcessId);
}
..........
ExFreePool(ParentName.Buffer);

����ֵ��
	�������ֵΪ STATUS_BUFFER_OVERFLOW����˵����ProcessImagePath��buffer���㣬
	���������µ�buffer.
*/
NTSTATUS
GetProcessImagePath(
	OUT PUNICODE_STRING ProcessImagePath,
	IN   HANDLE    dwProcessId
);