/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */

#include "linux_inc.h"

#include "manager.h"
#include "../../ioctl.h"

int co_os_manager_open(struct inode *inode, struct file *file)
{
	file->private_data = (void *)NULL;

        if (!try_module_get(THIS_MODULE))
		return -EBUSY;

	return 0;
}

static int co_os_manager_ioctl_buffer(co_linux_io_t *ioctl, char *buffer, struct file *file)
{
	co_rc_t rc;
	unsigned long return_size = 0;

	if (copy_from_user(buffer, ioctl->input_buffer, ioctl->input_buffer_size))
		return -EFAULT;

	rc = co_manager_ioctl(global_manager, 
			      ioctl->code, 
			      buffer, ioctl->input_buffer_size,
			      ioctl->output_buffer_size, &return_size,
			      &file->private_data);
	if (!CO_OK(rc))
		return -EIO;

	if (return_size) {
		if (copy_to_user(ioctl->output_buffer, buffer, return_size))
			return -EFAULT;
	}

	if (ioctl->output_returned != NULL) {
		if (copy_to_user(ioctl->output_returned, &return_size, sizeof(unsigned long))) {
			return -EFAULT;
		}
	}

	return 0;
}

int co_os_manager_ioctl(struct inode *inode, struct file *file, 
			unsigned int cmd, unsigned long arg)
{
	co_linux_io_t ioctl;
	unsigned long buffer_size;
	int ret = -1;

	if (cmd != CO_LINUX_IOCTL_ID)
		return -1;

	if (copy_from_user(&ioctl, (void *)arg, sizeof(ioctl)))
                return -EFAULT;

	buffer_size = ioctl.input_buffer_size;
	if (buffer_size < ioctl.output_buffer_size)
		buffer_size = ioctl.output_buffer_size;

	if (buffer_size > 0x400000)
		return -EIO;
	
	if (buffer_size > 80) {
		void *buffer = vmalloc(buffer_size);
		if (buffer == NULL) {
			    co_debug("ioctl buffer too big: %x\n", buffer_size);
			    return -ENOMEM;
		}

		ret = co_os_manager_ioctl_buffer(&ioctl, buffer, file);
		vfree(buffer);
	} else {
		char on_stack[80];
		ret = co_os_manager_ioctl_buffer(&ioctl, on_stack, file);
	}
	    
	return ret;
}

int co_os_manager_release(struct inode *inode, struct file *file)
{
	co_manager_cleanup(global_manager, &file->private_data);

	module_put(THIS_MODULE);

	return 0;
}

static struct file_operations manager_fileops = {
        .open           = co_os_manager_open,
	.ioctl          = co_os_manager_ioctl,
        .release        = co_os_manager_release,
};

co_rc_t co_os_manager_init(co_manager_t *manager, co_osdep_manager_t *osdep)
{
	co_rc_t rc = CO_RC(OK);
	co_osdep_manager_t dep;

	dep = (typeof(*osdep))(co_os_malloc(sizeof(**osdep)));
	*osdep = dep;
	if (*osdep == NULL)
		return CO_RC(ERROR);

	memset(dep, 0, sizeof(*dep));

	dep->proc_root = proc_mkdir("colinux", &proc_root);
	if (dep->proc_root == NULL) {
		rc = CO_RC(ERROR);
		goto error;
	}
	dep->proc_root->owner = THIS_MODULE;

	dep->proc_ioctl = create_proc_entry("ioctl",  S_IFREG|S_IRUSR|S_IWUSR, dep->proc_root);
	if (!dep->proc_ioctl) {
		rc = CO_RC(ERROR);
		goto error_root;
	}

	dep->proc_ioctl->proc_fops = &manager_fileops;
	dep->proc_ioctl->owner = THIS_MODULE;

	return rc;

error_root:
	remove_proc_entry("colinux", &proc_root);

error:
	co_os_free(dep);
	return rc;
}

void co_os_manager_free(co_osdep_manager_t osdep)
{
	remove_proc_entry("ioctl", osdep->proc_root);
	remove_proc_entry("colinux", &proc_root);
	co_os_free(osdep);
}
