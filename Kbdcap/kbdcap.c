//date:2017/4/13
//func:keybord capture

#include <wdm.h>

#define KEYBOARD_NAME L"\\Device\\KeyboardClass0"

typedef struct kcpDeviceExtension {
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_OBJECT LowerDeviceObject;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

VOID kcpUnload(IN PDRIVER_OBJECT driver);

NTSTATUS kcpDispatch(
	IN PDEVICE_OBJECT device,
	IN PIRP irp
);


NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT driver,
	IN PUNICODE_STRING reg_path
)
{
	NTSTATUS status;
	size_t i;

	PFILE_OBJECT fileobj = NULL;
	PDEVICE_OBJECT fltobj = NULL;
	PDEVICE_OBJECT oldobj = NULL;
	PDEVICE_EXTENSION devExt = NULL;

	UNICODE_STRING com_name = RTL_CONSTANT_STRING(KEYBOARD_NAME);

	//dispatch function
	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
		driver->MajorFunction[i] = kcpDispatch;
	driver->DriverUnload = kcpUnload;

	//get device object by name
	status = IoGetDeviceObjectPointer(
		&com_name,
		FILE_ALL_ACCESS,
		&fileobj,
		&oldobj
	);

	if (NT_SUCCESS(status))
		ObDereferenceObject(fileobj);

	if (oldobj == NULL)
	{
		status = STATUS_UNSUCCESSFUL;
		return status;
	}

	//create device
	status = IoCreateDevice(
		driver,
		sizeof(DEVICE_EXTENSION),
		NULL,
		oldobj->DeviceType,
		0,
		FALSE,
		&fltobj
	);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	devExt = (PDEVICE_EXTENSION)fltobj->DeviceExtension;
	devExt->DeviceObject = fltobj;

	//copy flags
	if (oldobj->Flags & DO_BUFFERED_IO)
		fltobj->Flags |= DO_BUFFERED_IO;
	if (oldobj->Flags & DO_DIRECT_IO)
		fltobj->Flags |= DO_DIRECT_IO;

	if (oldobj->Characteristics & FILE_DEVICE_SECURE_OPEN)
		fltobj->Characteristics |= FILE_DEVICE_SECURE_OPEN;
	fltobj->Flags |= DO_POWER_PAGABLE;

	//attach
	devExt->LowerDeviceObject = IoAttachDeviceToDeviceStack(
		fltobj,
		oldobj
	);
	if (devExt->LowerDeviceObject == NULL)
	{
		IoDeleteDevice(fltobj);
		status = STATUS_UNSUCCESSFUL;
		return status;
	}

	return STATUS_SUCCESS;
}

VOID kcpUnload(IN PDRIVER_OBJECT driver)
{
	PDEVICE_OBJECT fltobj = driver->DeviceObject;
	PDEVICE_OBJECT topdev = ((PDEVICE_EXTENSION)fltobj->DeviceExtension)->LowerDeviceObject;
	if (topdev != NULL)
		IoDetachDevice(topdev);
	if (fltobj != NULL)
		IoDeleteDevice(fltobj);
}

NTSTATUS kcpDispatch(IN PDEVICE_OBJECT device, IN PIRP irp)
{
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
	PDEVICE_EXTENSION devExt = (PDEVICE_EXTENSION)device->DeviceExtension;
	NTSTATUS status;
	ULONG i;

	if (stack->MajorFunction == IRP_MJ_WRITE)
	{
		ULONG len = stack->Parameters.Write.Length;
		PUCHAR buf = NULL;
		if (irp->MdlAddress != NULL)
			buf = (PUCHAR)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
		else
			buf = irp->UserBuffer;
		if (buf == NULL)
			buf = (PUCHAR)irp->AssociatedIrp.SystemBuffer;

		for (i = 0; i < len; i++)
		{
			KdPrint(("comcap: Send Data: %2x\r\n", buf[i]));
		}
	}
	IoSkipCurrentIrpStackLocation(irp);
	return IoCallDriver(devExt->LowerDeviceObject, irp);
}