/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 */

#include "fileblock.h"
#include "monitor.h"

#include <memory.h>

co_rc_t co_monitor_file_block_service(co_monitor_t *cmon,
				      co_block_dev_t *dev, 
				      co_block_request_t *request)
{ 
	co_monitor_file_block_dev_t *fdev = (co_monitor_file_block_dev_t *)dev;
	co_rc_t rc = CO_RC_ERROR;

	switch (request->type) { 
	case CO_BLOCK_OPEN: {
		if (fdev->state != CO_MONITOR_FILE_BLOCK_CLOSED) {
			co_debug_error("monitor: cobd not closed!\n");
			break;
		}

		co_debug("monitor: cobd opened (%s)\n", fdev->pathname);
			
		rc = fdev->op->open(cmon, fdev);
		if (CO_OK(rc))
			fdev->state = CO_MONITOR_FILE_BLOCK_OPENED;

		break;
	}

	case CO_BLOCK_READ: {
		if (fdev->state != CO_MONITOR_FILE_BLOCK_OPENED) {
			co_debug_error("monitor: read: cobd not open!\n");
			break;
		}

		rc = fdev->op->read(cmon, dev, fdev, request);
		break;
	}

	case CO_BLOCK_WRITE: {
		if (fdev->state != CO_MONITOR_FILE_BLOCK_OPENED) {
			co_debug_error("monitor: write: cobd not open!\n");
			break;
		}

		rc = fdev->op->write(cmon, dev, fdev, request);
		break;
	}

	case CO_BLOCK_CLOSE: {
		if (fdev->state != CO_MONITOR_FILE_BLOCK_OPENED) {
			co_debug_error("monitor: close: cobd not open!\n");
			break;
		}
	
		co_debug("monitor: cobd closed (%s)\n", fdev->pathname);

		fdev->op->close(fdev);
		fdev->state = CO_MONITOR_FILE_BLOCK_CLOSED;

		rc = CO_RC_OK;
		break;
	}
	case CO_BLOCK_STAT: {
		rc = fdev->op->get_size((co_monitor_file_block_dev_t *)dev, &fdev->dev.size);

		if (CO_OK(rc))
			request->disk_size = fdev->dev.size;

		return rc;
	}    
	default:
		break;
	}

	return rc;
}

co_rc_t co_monitor_file_block_init(co_monitor_file_block_dev_t *dev, 
				   co_pathname_t *pathname)
{
	memset(dev, 0, sizeof(*dev));
	memcpy(dev->pathname, pathname, sizeof(*pathname));

	dev->op = &co_os_file_block_default_operations;
	dev->state = CO_MONITOR_FILE_BLOCK_CLOSED;
	dev->dev.service = co_monitor_file_block_service;

	return CO_RC(OK);
}

void co_monitor_file_block_shutdown(co_monitor_file_block_dev_t *dev)
{
	if (dev->state == CO_MONITOR_FILE_BLOCK_OPENED) {
		dev->op->close(dev);
		dev->state = CO_MONITOR_FILE_BLOCK_CLOSED;
	}
}

