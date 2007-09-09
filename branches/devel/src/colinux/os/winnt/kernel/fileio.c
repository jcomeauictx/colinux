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

static co_rc_t status_convert(NTSTATUS status)
{
	switch (status) {
	case STATUS_NO_SUCH_FILE: 
	case STATUS_OBJECT_NAME_NOT_FOUND: return CO_RC(NOT_FOUND);
	case STATUS_CANNOT_DELETE:
	case STATUS_ACCESS_DENIED: return CO_RC(ACCESS_DENIED);
	case STATUS_INVALID_PARAMETER: return CO_RC(INVALID_PARAMETER);
	}

	if (status != STATUS_SUCCESS) {
		return CO_RC(ERROR);
	}

	return CO_RC(OK);
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
			      open_flags | SYNCHRONIZE,
			      &ObjectAttributes,
			      &IoStatusBlock,
			      NULL,
			      file_attribute,
			      (open_flags == FILE_LIST_DIRECTORY) ?
				 FILE_SHARE_DIRECTORY : 0,
			      create_disposition,
			      options | FILE_SYNCHRONOUS_IO_NONALERT,
			      NULL,
			      0);

	if (status != STATUS_SUCCESS)
		co_debug_lvl(filesystem, 5, "error %x ZwOpenFile('%s')", status, pathname);


	co_winnt_free_unicode(&unipath);

	return status_convert(status);
}

co_rc_t co_os_file_open(char *pathname, PHANDLE FileHandle, unsigned long open_flags)
{   
	return co_os_file_create(pathname, FileHandle, open_flags, 0, FILE_OPEN, 0);
}

co_rc_t co_os_file_close(PHANDLE FileHandle)
{    
	NTSTATUS status;

	status = ZwClose(FileHandle);

	return status_convert(status);
}

typedef struct {
	LARGE_INTEGER offset;
	HANDLE file_handle;
} co_os_transfer_file_block_data_t;

static co_rc_t transfer_file_block(co_monitor_t *cmon, 
				  void *host_data, void *linuxvm, 
				  unsigned long size, 
				  co_monitor_transfer_dir_t dir)
{
	IO_STATUS_BLOCK isb;
	NTSTATUS status;
	co_os_transfer_file_block_data_t *data;
	co_rc_t rc = CO_RC_OK;
	HANDLE FileHandle;

	data = (co_os_transfer_file_block_data_t *)host_data;

	FileHandle = data->file_handle;

	if (CO_MONITOR_TRANSFER_FROM_HOST == dir) {
		status = ZwReadFile(FileHandle,
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
		status = ZwWriteFile(FileHandle,
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
		co_debug_error("block io failed: %x %x (reason: %x)",
				linuxvm, size, status);
		rc = status_convert(status);
	}

	data->offset.QuadPart += size;

	return rc;
}

co_rc_t co_os_file_block_read_write(co_monitor_t *linuxvm,
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

	rc = co_monitor_host_linuxvm_transfer(linuxvm, 
					      &data, 
					      transfer_file_block,
					      address,
					      size,
					      (read ? CO_MONITOR_TRANSFER_FROM_HOST : 
					       CO_MONITOR_TRANSFER_FROM_LINUX));

	return rc;
}

typedef void (*co_os_change_file_info_func_t)(void *data, VOID *buffer, ULONG len);

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

	rc = co_os_file_open(filename, &handle, FILE_READ_DATA | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES);
	if (!CO_OK(rc)) {
		rc = co_os_file_open(filename, &handle, FILE_WRITE_ATTRIBUTES);
		if (!CO_OK(rc))
			return rc;
	}

	status = ZwQueryInformationFile(handle, io_status, buffer, len, info_class);
	if (status == STATUS_SUCCESS) {
		if (func) {
			func(data, buffer, len);
		}
		status = ZwSetInformationFile(handle, io_status, buffer, len, info_class);
	}

	co_os_file_close(handle);

	return status_convert(status);
}

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

	return status_convert(status);
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

static void change_file_info_func(void *data, VOID *buffer, ULONG len)
{
	struct fuse_attr *attr = (struct fuse_attr *)data;
	FILE_BASIC_INFORMATION *fbi = (FILE_BASIC_INFORMATION *)buffer;

	fbi->LastAccessTime = unix_time_to_windows_time(attr->mtime);
	fbi->LastWriteTime = unix_time_to_windows_time(attr->atime);
	KeQuerySystemTime(&fbi->ChangeTime);
}

co_rc_t co_os_file_set_attr(char *filename, unsigned long valid, struct fuse_attr *attr)
{
	IO_STATUS_BLOCK io_status;
	co_rc_t rc = CO_RC(OK);

	/* FIXME: make return codes not to overwrite each other */
	if (valid & FATTR_MODE) {
		rc = CO_RC(OK); /* TODO */
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
	char * dirname;
	char * filename;
	UNICODE_STRING dirname_unicode;
	UNICODE_STRING filename_unicode;
	OBJECT_ATTRIBUTES attributes;
	NTSTATUS status;
	HANDLE handle;
	struct {
		union {
			FILE_FULL_DIRECTORY_INFORMATION entry;
			FILE_BOTH_DIRECTORY_INFORMATION entry2;
		};
		WCHAR name_filler[sizeof(co_pathname_t)];
	} entry_buffer;
	IO_STATUS_BLOCK io_status;
	co_rc_t rc;
	int len, len1;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	ULONG FileAttributes;
 
	len = co_strlen(fullname);
	len1 = len;
	
	while (len > 0 && (fullname[len-1] != '\\'))
		len--;

	dirname = co_os_malloc(len+1);
	if (!dirname) {
		co_debug_lvl(filesystem, 5, "no memory");
		return CO_RC(OUT_OF_MEMORY);
	}

	memcpy(dirname, fullname, len);
	dirname[len] = 0;

	filename = &fullname[len];

	rc = co_winnt_utf8_to_unicode(dirname, &dirname_unicode);
	if (!CO_OK(rc))
		goto error_0;

	rc = co_winnt_utf8_to_unicode(filename, &filename_unicode);
	if (!CO_OK(rc))
		goto error_1;

        InitializeObjectAttributes(&attributes, &dirname_unicode,
                                   OBJ_CASE_INSENSITIVE, NULL, NULL);
 
        status = ZwCreateFile(&handle, FILE_LIST_DIRECTORY,
			      &attributes, &io_status, NULL, 0, FILE_SHARE_DIRECTORY, 
			      FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT | FILE_DIRECTORY_FILE, 
			      NULL, 0);

	if (!NT_SUCCESS(status)) {
		co_debug_lvl(filesystem, 5, "error %x ZwCreateFile('%s')", status, dirname);
                rc = status_convert(status);
                goto error_2;
        }
 
        status = ZwQueryDirectoryFile(handle, NULL, NULL, 0, &io_status, 
                                      &entry_buffer, sizeof(entry_buffer), 
                                      FileFullDirectoryInformation, PTRUE, &filename_unicode, 
                                      PTRUE);
        if (!NT_SUCCESS(status)) {
		if (status == STATUS_UNMAPPABLE_CHARACTER) {
			status = ZwQueryDirectoryFile(handle, NULL, NULL, 0, &io_status, 
						      &entry_buffer, sizeof(entry_buffer), 
						      FileBothDirectoryInformation, PTRUE, &filename_unicode, 
						      PTRUE);
			
			if (!NT_SUCCESS(status)) {
				co_debug_lvl(filesystem, 5, "error %x ZwQueryDirectoryFile('%s')", status, filename);
				rc = status_convert(status);
				goto error_3;
			} else {
				LastAccessTime = entry_buffer.entry2.LastAccessTime;
				LastWriteTime = entry_buffer.entry2.LastWriteTime;
				ChangeTime = entry_buffer.entry2.ChangeTime;
				EndOfFile = entry_buffer.entry2.EndOfFile;
				FileAttributes = entry_buffer.entry2.FileAttributes;
			}
		} else {
			co_debug_lvl(filesystem, 5, "error %x ZwQueryDirectoryFile('%s')", status, filename);
			rc = status_convert(status);
			goto error_3;
		}
        } else {
		LastAccessTime = entry_buffer.entry.LastAccessTime;
		LastWriteTime = entry_buffer.entry.LastWriteTime;
		ChangeTime = entry_buffer.entry.ChangeTime;
		EndOfFile = entry_buffer.entry.EndOfFile;
		FileAttributes = entry_buffer.entry.FileAttributes;
	}
 
        attr->uid = 0;
        attr->gid = 0;
        attr->rdev = 0;
        attr->_dummy = 0;
 
        attr->atime = windows_time_to_unix_time(LastAccessTime);
        attr->mtime = windows_time_to_unix_time(LastWriteTime);
        attr->ctime = windows_time_to_unix_time(ChangeTime);
 
        attr->mode = FUSE_S_IRWXU | FUSE_S_IRGRP | FUSE_S_IROTH;
 
	/* Hack: WinNT detects "C:\" not as directory! */
        if ((FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
	    (len1 >= 3 && len == len1 && fullname [len1-1] == '\\'))
                attr->mode |= FUSE_S_IFDIR;
        else
                attr->mode |= FUSE_S_IFREG;

	attr->nlink = 1;
        attr->size = EndOfFile.QuadPart;
        attr->blocks = (EndOfFile.QuadPart + ((1<<10)-1)) >> 10;
	
        rc = CO_RC(OK);

error_3:
        ZwClose(handle);
error_2:
	co_winnt_free_unicode(&filename_unicode);
error_1:
	co_winnt_free_unicode(&dirname_unicode);
error_0:
	co_os_free(dirname);
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

		co_debug_lvl(filesystem, 5, "error %x ZwDeleteFile('%s')", status, filename);
	}

	co_winnt_free_unicode(&unipath);

	return status_convert(status);
}

co_rc_t co_os_file_rmdir(char *filename)
{
	return co_os_file_unlink(filename);
}

co_rc_t co_os_file_mkdir(char *dirname)
{
	HANDLE handle;
	co_rc_t rc;

	rc = co_os_file_create(dirname, &handle, FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
		FILE_ATTRIBUTE_DIRECTORY, FILE_CREATE, FILE_DIRECTORY_FILE);
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

	rc = co_os_file_open(filename, &handle, FILE_READ_DATA | FILE_WRITE_DATA);
	if (!CO_OK(rc))
		goto error;

	rename_info->ReplaceIfExists = TRUE;
	rename_info->RootDirectory = NULL;
	rename_info->FileNameLength = char_count * sizeof(WCHAR);

	rc = co_utf8_mbstowcs(rename_info->FileName, dest_filename, char_count + 1);
	if (!CO_OK(rc))
		goto error2;
	
	co_debug_lvl(filesystem, 10, "rename of '%s' to '%s'\n", filename, dest_filename);

	status = ZwSetInformationFile(handle, &io_status, rename_info, block_size,
				      FileRenameInformation);
	rc = status_convert(status);

	if (!CO_OK(rc))
		co_debug_lvl(filesystem, 5, "error %x ZwSetInformationFile rename %s,%s\n", status, filename, dest_filename);

error2:
	co_os_file_close(handle);

error:
	co_os_free(rename_info);
	return rc;
}

co_rc_t co_os_file_mknod(char *filename)
{
	co_rc_t rc;
	HANDLE handle;

	rc = co_os_file_create(filename, &handle, FILE_READ_DATA | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES,
			       FILE_ATTRIBUTE_NORMAL, FILE_CREATE, 0);

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

	co_debug_lvl(filesystem, 10, "listing of '%s'\n", dirname);

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
		co_debug_lvl(filesystem, 5, "error %x ZwCreateFile('%s')", status, dirname);
		rc = status_convert(status);
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
		rc = co_os_file_create(pathname, &handle, 
				       FILE_LIST_DIRECTORY, 0,
				       FILE_OPEN, FILE_DIRECTORY_FILE | FILE_OPEN_FOR_FREE_SPACE_QUERY);
		
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

	rc = status_convert(status);

	co_os_file_close(handle);
	return rc;
}
