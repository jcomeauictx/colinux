/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 

#include "ddk.h"
#include <ddk/ntifs.h>
#include <ddk/ntdddisk.h>

#include <colinux/os/alloc.h>
#include <colinux/common/libc.h>
#include <colinux/common/unicode.h>
#include <colinux/kernel/transfer.h>
#include <colinux/kernel/fileblock.h>
#include <colinux/kernel/monitor.h>
#include <colinux/kernel/filesystem.h>
#include <colinux/os/kernel/time.h>
#include <colinux/os/kernel/filesystem.h>

#include "time.h"
#include "fileio.h"

#define FILE_SHARE_DIRECTORY (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)

typedef struct {
	co_monitor_t *monitor;
	co_pfn_t pfn;
	unsigned char *page;
	unsigned char *start;
	struct {
		co_message_t message;
		co_linux_message_t linux_message;
		co_block_intr_t intr;
	} msg;
	IO_STATUS_BLOCK isb;
	LARGE_INTEGER offset;
	unsigned long size;
} callback_context_t;


typedef struct {
	HANDLE file_handle;
	LARGE_INTEGER offset;
} co_os_transfer_file_block_data_t;

co_rc_t co_winnt_utf8_to_unicode(const char *src, UNICODE_STRING *unicode_str)
{
	co_rc_t rc;
	co_wchar_t *wstring;
	unsigned long size;

	unicode_str->Buffer = NULL;

	rc = co_utf8_dup_to_wc(src, &wstring, &size);
	if (!CO_OK(rc))
		return rc;

	unicode_str->Length = size * sizeof(WCHAR);
	unicode_str->MaximumLength = (size + 1) * sizeof(WCHAR);
	unicode_str->Buffer = wstring;

	return CO_RC(OK);
}

void co_winnt_free_unicode(UNICODE_STRING *unicode_str)
{
	if (!unicode_str->Buffer)
		return;

	co_utf8_free_wc(unicode_str->Buffer);
	unicode_str->Buffer = NULL;
}

co_rc_t co_status_convert(NTSTATUS status)
{
	switch (status) {
	case STATUS_PENDING:
	case STATUS_SUCCESS: return CO_RC(OK);
	case STATUS_NO_SUCH_FILE: 
	case STATUS_OBJECT_NAME_NOT_FOUND: return CO_RC(NOT_FOUND);
	case STATUS_CANNOT_DELETE:
	case STATUS_ACCESS_DENIED: return CO_RC(ACCESS_DENIED);
	case STATUS_INVALID_PARAMETER: return CO_RC(INVALID_PARAMETER);
	default: return CO_RC(ERROR);
	}
}

co_rc_t co_os_file_create(char *pathname, PHANDLE FileHandle, unsigned long open_flags,
			  unsigned long file_attribute, unsigned long create_disposition,
			  unsigned long options)
{    
	NTSTATUS status;
	OBJECT_ATTRIBUTES ObjectAttributes;
	IO_STATUS_BLOCK IoStatusBlock;
	UNICODE_STRING unipath;
	co_rc_t rc;

	rc = co_winnt_utf8_to_unicode(pathname, &unipath);
	if (!CO_OK(rc))
		return rc;

	InitializeObjectAttributes(&ObjectAttributes, 
				   &unipath,
				   OBJ_CASE_INSENSITIVE,
				   NULL,
				   NULL);

	status = ZwCreateFile(FileHandle, 
			      open_flags,
			      &ObjectAttributes,
			      &IoStatusBlock,
			      NULL,
			      file_attribute,
			      (open_flags == (FILE_LIST_DIRECTORY | SYNCHRONIZE)) ?
				 FILE_SHARE_DIRECTORY : 0,
			      create_disposition,
			      options,
			      NULL,
			      0);

	if (status != STATUS_SUCCESS)
		co_debug_lvl(filesystem, 5, "error %x ZwOpenFile('%s')", (int)status, pathname);

	co_winnt_free_unicode(&unipath);

	return co_status_convert(status);
}

co_rc_t co_os_file_open(char *pathname, PHANDLE FileHandle, unsigned long open_flags)
{   
	return co_os_file_create(pathname, FileHandle, open_flags | SYNCHRONIZE, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT);
}

co_rc_t co_os_file_close(PHANDLE FileHandle)
{    
	NTSTATUS status;

	status = ZwClose(FileHandle);

	return co_status_convert(status);
}

static co_rc_t transfer_file_block(co_monitor_t *cmon, 
				  void *host_data, void *linuxvm, 
				  unsigned long size, 
				  co_monitor_transfer_dir_t dir)
{
	IO_STATUS_BLOCK isb;
	NTSTATUS status;
	co_os_transfer_file_block_data_t *data;
	co_rc_t rc = CO_RC_OK;

	data = (co_os_transfer_file_block_data_t *)host_data;

	if (CO_MONITOR_TRANSFER_FROM_HOST == dir) {
		status = ZwReadFile(data->file_handle,
				    NULL,
				    NULL,
				    NULL, 
				    &isb,
				    linuxvm,
				    size,
				    &data->offset,
				    NULL);
	}
	else {
		status = ZwWriteFile(data->file_handle,
				     NULL,
				     NULL,
				     NULL, 
				     &isb,
				     linuxvm,
				     size,
				     &data->offset,
				     NULL);
	}

	if (status != STATUS_SUCCESS) {
		co_debug_error("block io failed: %p %lx (reason: %x)",
				linuxvm, size, (int)status);
		rc = co_status_convert(status);
	}

	data->offset.QuadPart += size;

	return rc;
}

co_rc_t co_os_file_block_read_write(co_monitor_t *monitor,
				    HANDLE file_handle,
				    unsigned long long offset,
				    vm_ptr_t address,
				    unsigned long size,
				    bool_t read)
{
	co_rc_t rc;
	co_os_transfer_file_block_data_t data;
	
	data.offset.QuadPart = offset;
	data.file_handle = file_handle;

	rc = co_monitor_host_linuxvm_transfer(monitor,
					      &data,
					      transfer_file_block,
					      address,
					      size,
					      (read ? CO_MONITOR_TRANSFER_FROM_HOST :
					       CO_MONITOR_TRANSFER_FROM_LINUX));

	return rc;
}

static void CALLBACK transfer_file_block_callback(callback_context_t *context, PIO_STATUS_BLOCK IoStatusBlock, ULONG Reserved)
{
	co_debug_lvl(filesystem, 10, "cobd%d callback size=%ld info=%ld status=%X",
			context->msg.linux_message.unit, context->size,
			(long)IoStatusBlock->Information, (int)IoStatusBlock->Status);

	if (IoStatusBlock->Status == STATUS_SUCCESS)
		context->msg.intr.uptodate = 1;
	else
		co_debug("cobd%d callback failed size=%ld info=%ld status=%X",
			    context->msg.linux_message.unit, context->size,
			    (unsigned long)IoStatusBlock->Information,
			    (int)IoStatusBlock->Status);

	co_monitor_host_linuxvm_transfer_unmap(context->monitor, context->page, context->pfn);
	co_monitor_message_from_user(context->monitor, 0, &context->msg.message);
	co_os_free(context);
}

co_rc_t co_os_file_block_async_read_write(co_monitor_t *monitor,
				    HANDLE file_handle,
				    unsigned long long offset,
				    vm_ptr_t address,
				    unsigned long size,
				    bool_t read,
				    int unit,
				    void *irq_request)
{
	co_rc_t rc;
	NTSTATUS status;

	// Prepare callback
	callback_context_t *context = co_os_malloc(sizeof (callback_context_t));
	if (!context)
		return CO_RC(OUT_OF_MEMORY);
	co_memset(context, 0, sizeof(callback_context_t));

	context->monitor = monitor;
	context->offset.QuadPart = offset;
	context->size = size;
	context->msg.message.from = CO_MODULE_COBD0 + unit;
	context->msg.message.to = CO_MODULE_LINUX;
	context->msg.message.priority = CO_PRIORITY_DISCARDABLE;
	context->msg.message.type = CO_MESSAGE_TYPE_OTHER;
	context->msg.message.size = sizeof (context->msg) - sizeof (context->msg.message);
	context->msg.linux_message.device = CO_DEVICE_BLOCK;
	context->msg.linux_message.unit = unit;
	context->msg.linux_message.size = sizeof (context->msg.intr);
	context->msg.intr.irq_request = irq_request;

	// map linux kernal memory into host memory
	rc = co_monitor_host_linuxvm_transfer_map(
		monitor,
		address,
		size,
		&context->start,
		&context->page,
		&context->pfn);

	if (CO_OK(rc)) {
		if (read) {
			status = ZwReadFile(file_handle,
				    NULL,
				    (PIO_APC_ROUTINE) transfer_file_block_callback,
				    context,
				    &context->isb,
				    context->start,
				    size,
				    &context->offset,
				    NULL);
			co_debug_lvl(filesystem, 10, "cobd%d read status %X", unit, (int)status);
		} else {
			status = ZwWriteFile(file_handle,
				     NULL,
				     (PIO_APC_ROUTINE) transfer_file_block_callback,
				     context,
				     &context->isb,
				     context->start,
				     size,
				     &context->offset,
				     NULL);
			co_debug_lvl(filesystem, 10, "cobd%d write status %X", unit, (int)status);
		}

		if (status == STATUS_PENDING || status == STATUS_SUCCESS)
			return CO_RC(OK);
		
		rc = co_status_convert(status);
		co_debug("block io failed: %p %lx (reason: %x)", context->start, size, (int)status);
		co_monitor_host_linuxvm_transfer_unmap(monitor, context->page, context->pfn);
	}

	co_os_free(context);
	return rc;
}

typedef void (*co_os_change_file_info_func_t)(void *data, VOID *buffer, ULONG len);

static
co_rc_t co_os_change_file_information(char *filename,
				      IO_STATUS_BLOCK *io_status,
				      VOID *buffer,
				      ULONG len,
				      FILE_INFORMATION_CLASS info_class,
				      co_os_change_file_info_func_t func,
				      void *data)
{
	NTSTATUS status;
	HANDLE handle;
	co_rc_t rc;

	rc = co_os_file_open(filename, &handle, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES);
	if (!CO_OK(rc))
		return rc;

	status = ZwQueryInformationFile(handle, io_status, buffer, len, info_class);
	if (status == STATUS_SUCCESS) {
		if (func) {
			func(data, buffer, len);
		}
		status = ZwSetInformationFile(handle, io_status, buffer, len, info_class);
	}

	co_os_file_close(handle);

	return co_status_convert(status);
}

static
co_rc_t co_os_set_file_information(char *filename,
				   IO_STATUS_BLOCK *io_status,
				   VOID *buffer,
				   ULONG len,
				   FILE_INFORMATION_CLASS info_class)
{
	HANDLE handle;
	NTSTATUS status;
	co_rc_t rc;

	rc = co_os_file_open(filename, &handle, FILE_READ_DATA | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES);
	if (!CO_OK(rc))
		return rc;

	status = ZwSetInformationFile(handle, io_status, buffer, len, info_class);

	co_os_file_close(handle);

	return co_status_convert(status);
}

co_rc_t co_os_file_read_write(co_monitor_t *linuxvm, char *filename, 
			      unsigned long long offset, unsigned long size,
			      vm_ptr_t src_buffer, bool_t read)
{
	co_rc_t rc;
	HANDLE handle;

	rc = co_os_file_open(filename, &handle, read ? FILE_READ_DATA : FILE_WRITE_DATA);
	if (!CO_OK(rc))
		return rc;

	rc = co_os_file_block_read_write(linuxvm,
					 handle,
					 offset,
					 src_buffer,
					 size,
					 read);

	co_os_file_close(handle);

	return rc;
}

static void change_file_mode_func(void *data, VOID *buffer, ULONG len)
{
	struct fuse_attr *attr = (struct fuse_attr *)data;
	FILE_BASIC_INFORMATION *fbi = (FILE_BASIC_INFORMATION *)buffer;

	if (attr->mode & FUSE_S_IRUSR)
		fbi->FileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;
	else
		fbi->FileAttributes |= FILE_ATTRIBUTE_HIDDEN;

	if (attr->mode & FUSE_S_IWUSR)
		fbi->FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
	else
		fbi->FileAttributes |= FILE_ATTRIBUTE_READONLY;

	if (attr->mode & FUSE_S_IFDIR)
		if (attr->mode & FUSE_S_IXUSR)
			fbi->FileAttributes &= ~FILE_ATTRIBUTE_SYSTEM;
		else
			fbi->FileAttributes |= FILE_ATTRIBUTE_SYSTEM;
	else
		if (attr->mode & FUSE_S_IXUSR)
			fbi->FileAttributes |= FILE_ATTRIBUTE_SYSTEM;
		else
			fbi->FileAttributes &= ~FILE_ATTRIBUTE_SYSTEM;
}

static void change_file_info_func(void *data, VOID *buffer, ULONG len)
{
	struct fuse_attr *attr = (struct fuse_attr *)data;
	FILE_BASIC_INFORMATION *fbi = (FILE_BASIC_INFORMATION *)buffer;

	fbi->LastAccessTime = unix_time_to_windows_time(attr->atime);
	fbi->LastWriteTime = unix_time_to_windows_time(attr->mtime);
	KeQuerySystemTime(&fbi->ChangeTime);
}

co_rc_t co_os_file_set_attr(char *filename, unsigned long valid, struct fuse_attr *attr)
{
	IO_STATUS_BLOCK io_status;
	co_rc_t rc = CO_RC(OK);

	/* FIXME: make return codes not to overwrite each other */
	if (valid & FATTR_MODE) {
		FILE_BASIC_INFORMATION fbi;

		rc = co_os_change_file_information(filename, &io_status, &fbi,
						   sizeof(fbi), FileBasicInformation,
						   change_file_mode_func,
						   attr);
	}

	if (valid & FATTR_UID) {
		rc = CO_RC(OK); /* TODO */
	}

	if (valid & FATTR_GID) {
		rc = CO_RC(OK); /* TODO */
	}

	if (valid & FATTR_UTIME) {
		FILE_BASIC_INFORMATION fbi;

		rc = co_os_change_file_information(filename, &io_status, &fbi,
						   sizeof(fbi), FileBasicInformation,
						   change_file_info_func,
						   attr);
	}

	if (valid & FATTR_SIZE) {
		FILE_END_OF_FILE_INFORMATION feofi;
		
		feofi.EndOfFile.QuadPart = attr->size;

		rc = co_os_set_file_information(filename, &io_status, &feofi,
						sizeof(feofi), FileEndOfFileInformation);
	} 

	return rc;
} 

co_rc_t co_os_file_get_attr(char *fullname, struct fuse_attr *attr)
{
	NTSTATUS status;
	HANDLE handle;
	IO_STATUS_BLOCK io_status;
	FILE_STANDARD_INFORMATION fsi;
	FILE_BASIC_INFORMATION fbi;
	co_rc_t rc;
	int len;

	attr->uid = 0;
	attr->gid = 0;
	attr->rdev = 0;
	attr->_dummy = 0;

	len = co_strlen(fullname);

	/* Hack: WinNT detects "C:\" not as directory! */
	if (len >= 3 && fullname[len-1] == ':') {
		co_debug_lvl(filesystem, 10, "Root dir: '%s'", fullname);

		attr->atime = \
		attr->mtime = \
		attr->ctime = co_os_get_time();

		attr->mode = FUSE_S_IFDIR | 0777;

		attr->size = 0;
		attr->blocks = 0;
		attr->nlink = 1;

		return CO_RC(OK);
	}

	rc = co_os_file_open(fullname, &handle, FILE_READ_ATTRIBUTES);
	if (!CO_OK(rc))
		return rc;

	status = ZwQueryInformationFile(handle, &io_status,
					&fsi, sizeof(fsi),
					FileStandardInformation);

	if (!NT_SUCCESS(status)) {
		co_debug_lvl(filesystem, 5, "error %x FileStandardInformation('%s')", (int)status, fullname);
		rc = co_status_convert(status);
		goto error_1;
	}

	attr->nlink = fsi.NumberOfLinks;
	attr->size = fsi.EndOfFile.QuadPart;
	attr->blocks = (fsi.EndOfFile.QuadPart + ((1<<10)-1)) >> 10;

	status = ZwQueryInformationFile(handle,	&io_status,
					&fbi, sizeof(fbi),
					FileBasicInformation);

	if (!NT_SUCCESS(status)) {
		co_debug_lvl(filesystem, 5, "error %x FileBasicInformation('%s')", (int)status, fullname);
		rc = co_status_convert(status);
		goto error_1;
	}

	attr->atime = windows_time_to_unix_time(fbi.LastAccessTime);
	attr->mtime = windows_time_to_unix_time(fbi.LastWriteTime);
	attr->ctime = windows_time_to_unix_time(fbi.ChangeTime);

	#define FUSE_S_IR (FUSE_S_IRUSR | FUSE_S_IRGRP | FUSE_S_IROTH)
	#define FUSE_S_IW (FUSE_S_IWUSR | FUSE_S_IWGRP | FUSE_S_IWOTH)
	#define FUSE_S_IX (FUSE_S_IXUSR | FUSE_S_IXGRP | FUSE_S_IXOTH)

	if (fbi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		attr->mode = FUSE_S_IFDIR
			   | ((fbi.FileAttributes & FILE_ATTRIBUTE_HIDDEN)   ? 0 : FUSE_S_IR)
			   | ((fbi.FileAttributes & FILE_ATTRIBUTE_READONLY) ? 0 : FUSE_S_IW)
			   | ((fbi.FileAttributes & FILE_ATTRIBUTE_SYSTEM)   ? 0 : FUSE_S_IX);
	else
		attr->mode = FUSE_S_IFREG
			   | ((fbi.FileAttributes & FILE_ATTRIBUTE_HIDDEN)   ? 0 : FUSE_S_IR)
			   | ((fbi.FileAttributes & FILE_ATTRIBUTE_READONLY) ? 0 : FUSE_S_IW)
			   | ((fbi.FileAttributes & FILE_ATTRIBUTE_SYSTEM)   ? FUSE_S_IX : 0);

error_1:
	ZwClose(handle);
	return rc;
}

static void remove_read_only_func(void *data, VOID *buffer, ULONG len)
{
	FILE_BASIC_INFORMATION *fbi = (FILE_BASIC_INFORMATION *)buffer;

	fbi->FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
}

co_rc_t co_os_file_unlink(char *filename)
{
	OBJECT_ATTRIBUTES ObjectAttributes;
	UNICODE_STRING unipath;
	NTSTATUS status;
	bool_t tried_read_only_removal = PFALSE;
	co_rc_t rc;

	rc = co_winnt_utf8_to_unicode(filename, &unipath);
	if (!CO_OK(rc))
		return rc;

	InitializeObjectAttributes(&ObjectAttributes, 
				   &unipath,
				   OBJ_CASE_INSENSITIVE,
				   NULL,
				   NULL);

retry:
	status = ZwDeleteFile(&ObjectAttributes);
	if (status != STATUS_SUCCESS) {
		if (!tried_read_only_removal) {
			FILE_BASIC_INFORMATION fbi;
			IO_STATUS_BLOCK io_status;
			co_rc_t rc;
			
			rc = co_os_change_file_information(filename, &io_status, &fbi,
							   sizeof(fbi), FileBasicInformation,
							   remove_read_only_func,
							   NULL);

			tried_read_only_removal = PTRUE;
			if (CO_RC(OK))
				goto retry;
		}

		co_debug_lvl(filesystem, 5, "error %x ZwDeleteFile('%s')", (int)status, filename);
	}

	co_winnt_free_unicode(&unipath);

	return co_status_convert(status);
}

co_rc_t co_os_file_rmdir(char *filename)
{
	return co_os_file_unlink(filename);
}

co_rc_t co_os_file_mkdir(char *dirname)
{
	HANDLE handle;
	co_rc_t rc;

	rc = co_os_file_create(dirname, &handle, FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE,
		FILE_ATTRIBUTE_DIRECTORY, FILE_CREATE, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT);
	if (CO_OK(rc)) {
		co_os_file_close(handle);
	}

	return rc;
}

co_rc_t co_os_file_rename(char *filename, char *dest_filename)
{
	NTSTATUS status;
	IO_STATUS_BLOCK io_status;
	FILE_RENAME_INFORMATION *rename_info;
	HANDLE handle;
	int block_size;
	int char_count;
	co_rc_t rc;

	char_count = co_utf8_mbstrlen(dest_filename);
	block_size = (char_count + 1)*sizeof(WCHAR) + sizeof(FILE_RENAME_INFORMATION);
	rename_info = co_os_malloc(block_size);
	if (!rename_info)
		return CO_RC(OUT_OF_MEMORY);

	rc = co_os_file_open(filename, &handle, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES);
	if (!CO_OK(rc))
		goto error;

	rename_info->ReplaceIfExists = TRUE;
	rename_info->RootDirectory = NULL;
	rename_info->FileNameLength = char_count * sizeof(WCHAR);

	rc = co_utf8_mbstowcs(rename_info->FileName, dest_filename, char_count + 1);
	if (!CO_OK(rc))
		goto error2;
	
	co_debug_lvl(filesystem, 10, "rename of '%s' to '%s'", filename, dest_filename);

	status = ZwSetInformationFile(handle, &io_status, rename_info, block_size,
				      FileRenameInformation);
	rc = co_status_convert(status);

	if (!CO_OK(rc))
		co_debug_lvl(filesystem, 5, "error %x ZwSetInformationFile rename %s,%s", (int)status, filename, dest_filename);

error2:
	co_os_file_close(handle);

error:
	co_os_free(rename_info);
	return rc;
}

co_rc_t co_os_file_mknod(co_filesystem_t *filesystem, char *filename, unsigned long mode)
{
	co_rc_t rc;
	HANDLE handle;
	ULONG FileAttributes = FILE_ATTRIBUTE_NORMAL;

	if (!(filesystem->flags & COFS_MOUNT_NOATTRIB)) {
		if (!(mode & FUSE_S_IRUSR))
			FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
		if (!(mode & FUSE_S_IWUSR))
			FileAttributes |= FILE_ATTRIBUTE_READONLY;
		if (mode & FUSE_S_IXUSR)
			FileAttributes |= FILE_ATTRIBUTE_SYSTEM;
	}

	rc = co_os_file_create(filename, &handle, FILE_READ_DATA | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE,
			       FileAttributes, FILE_CREATE, FILE_SYNCHRONOUS_IO_NONALERT);

	if (CO_OK(rc)) {
		co_os_file_close(handle);
	}

	return rc;
}

co_rc_t co_os_file_getdir(char *dirname, co_filesystem_dir_names_t *names)
{
	UNICODE_STRING dirname_unicode;
	OBJECT_ATTRIBUTES attributes;
	NTSTATUS status;
	HANDLE handle;
	FILE_DIRECTORY_INFORMATION *dir_entries_buffer, *entry;
	unsigned long dir_entries_buffer_size = 0x1000;
	IO_STATUS_BLOCK io_status;
	BOOLEAN first_iteration = TRUE;
	co_filesystem_name_t *new_name;
	co_rc_t rc;

	co_list_init(&names->list);

	co_debug_lvl(filesystem, 10, "listing of '%s'", dirname);

	rc = co_winnt_utf8_to_unicode(dirname, &dirname_unicode);
	if (!CO_OK(rc))
		return rc;

	InitializeObjectAttributes(&attributes, &dirname_unicode,
				   OBJ_CASE_INSENSITIVE, NULL, NULL);

	status = ZwCreateFile(&handle, FILE_LIST_DIRECTORY,
			      &attributes, &io_status, NULL, 0, FILE_SHARE_DIRECTORY, 
			      FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_DIRECTORY_FILE, 
			      NULL, 0);
	
	if (!NT_SUCCESS(status)) {
		co_debug_lvl(filesystem, 5, "error %x ZwCreateFile('%s')", (int)status, dirname);
		rc = co_status_convert(status);
		goto error;
	}

	dir_entries_buffer = co_os_malloc(dir_entries_buffer_size);
	if (!dir_entries_buffer) {
		rc = CO_RC(OUT_OF_MEMORY);
		goto error_1;
	}

	for (;;) {
		status = ZwQueryDirectoryFile(handle, NULL, NULL, 0, &io_status, 
					      dir_entries_buffer, dir_entries_buffer_size, 
					      FileDirectoryInformation, FALSE, NULL, first_iteration);
		if (!NT_SUCCESS(status))
			break;

		entry = dir_entries_buffer;
  
		for (;;) {
			int filename_utf8_length;
			
			filename_utf8_length = co_utf8_wctowbstrlen(entry->FileName, entry->FileNameLength/sizeof(WCHAR));

			new_name = co_os_malloc(filename_utf8_length + sizeof(co_filesystem_name_t) + 2);
			if (!new_name) {
				rc = CO_RC(OUT_OF_MEMORY);
				goto error_2;
			}
			
			if (entry->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				new_name->type = FUSE_DT_DIR;
			else
				new_name->type = FUSE_DT_REG;

			rc = co_utf8_wcstombs(new_name->name, entry->FileName, filename_utf8_length + 1);
			if (!CO_OK(rc)) {
				co_os_free(new_name);
				goto error_2;
			}

			co_list_add_tail(&new_name->node, &names->list);
			if (entry->NextEntryOffset == 0)
				break;
			
			entry = (FILE_DIRECTORY_INFORMATION *)(((char *)entry) + entry->NextEntryOffset);
		}

		first_iteration = FALSE;
	}


	rc = CO_RC(OK);

error_2:
	if (!CO_OK(rc))
		co_filesystem_getdir_free(names);

	co_os_free(dir_entries_buffer);

error_1:
	ZwClose(handle);
error:
	co_winnt_free_unicode(&dirname_unicode);
	return rc;
}

co_rc_t co_os_file_fs_stat(co_filesystem_t *filesystem, struct fuse_statfs_out *statfs)
{
	FILE_FS_FULL_SIZE_INFORMATION fsi;
	HANDLE handle;
	NTSTATUS status;
	IO_STATUS_BLOCK io_status;
	co_pathname_t pathname;
	co_rc_t rc;
	int len;
	int loop = 2;
	
	memcpy(&pathname, &filesystem->base_path, sizeof(co_pathname_t));
	co_os_fs_add_last_component(&pathname);

	len = strlen(pathname);
	do {
		rc = co_os_file_create(pathname,
				&handle, 
				FILE_LIST_DIRECTORY | SYNCHRONIZE,
				0,
				FILE_OPEN,
				FILE_DIRECTORY_FILE | FILE_OPEN_FOR_FREE_SPACE_QUERY | FILE_SYNCHRONOUS_IO_NONALERT);
		
		if (CO_OK(rc)) 
			break;
		
		while (len > 0  &&  pathname[len-1] != '\\')
			len--;

		pathname[len] = '\0';
	} while (len > 0  &&  --loop);

	if (!CO_OK(rc))
		return rc;

	status = ZwQueryVolumeInformationFile(handle, &io_status, &fsi,
					      sizeof(fsi), FileFsFullSizeInformation);

	if (NT_SUCCESS(status)) {
		statfs->st.block_size = fsi.SectorsPerAllocationUnit * fsi.BytesPerSector;
		statfs->st.blocks = fsi.TotalAllocationUnits.QuadPart;
		statfs->st.blocks_free = fsi.CallerAvailableAllocationUnits.QuadPart;
		statfs->st.files = 0;
		statfs->st.files_free = 0;
		statfs->st.namelen = sizeof(co_pathname_t);
	}

	rc = co_status_convert(status);

	co_os_file_close(handle);
	return rc;
}
