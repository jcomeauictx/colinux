/*
 * This source code is a part of coLinux source package.
 *
 * Dan Aloni <da-x@colinux.org>, 2003 (c)
 *
 * The code is licensed under the GPL. See the COPYING file at
 * the root directory.
 *
 */ 
#include <string.h>
#include <mxml.h>

#include <colinux/common/libc.h>
#include <colinux/common/config.h>
#include <colinux/user/cmdline.h>
#include <colinux/os/user/misc.h>
#include <colinux/os/user/cobdpath.h>
#include "macaddress.h"

#include "daemon.h"

co_rc_t co_load_config_blockdev(co_config_t *out_config, mxml_element_t *element)
{
	int i;
	long index = -1;
	char *path = "";
	char *alias = NULL;
	char *enabled = NULL;
	co_block_dev_desc_t *blockdev;

	for (i=0; i < element->num_attrs; i++) {
		mxml_attr_t *attr = &element->attrs[i];

		if (strcmp(attr->name, "index") == 0)
			index = atoi(attr->value);

		if (strcmp(attr->name, "path") == 0)
			path = attr->value;

		if (strcmp(attr->name, "enabled") == 0)
			enabled = attr->value;

		if (strcmp(attr->name, "alias") == 0)
			alias = attr->value;
	}
	
	if (index < 0) {
		co_debug("config: invalid block_dev element: bad index\n");
		return CO_RC(ERROR);
	}

	if (index >= CO_MODULE_MAX_COBD) {
		co_debug("config: invalid block_dev element: bad index\n");
		return CO_RC(ERROR);
	}

	if (path == NULL) {
		co_debug("config: invalid block_dev element: bad path\n");
		return CO_RC(ERROR);
	}

	blockdev = &out_config->block_devs[index];
	
	snprintf(blockdev->pathname, sizeof(blockdev->pathname), "%s", path);
	blockdev->enabled = enabled ? (strcmp(enabled, "true") == 0) : 0;

	snprintf(blockdev->alias, sizeof(blockdev->alias), "%s", alias);
	blockdev->alias_used = alias != NULL;

	return CO_RC(OK);
}

co_rc_t co_load_config_image(co_config_t *out_config, mxml_element_t *element)
{
	int i;
	char *path = "";

	for (i=0; i < element->num_attrs; i++) {
		mxml_attr_t *attr = &element->attrs[i];

		if (strcmp(attr->name, "path") == 0)
			path = attr->value;
	}
	
	if (path == NULL) {
		co_debug("config: invalid image element: bad path\n");
		return CO_RC(ERROR);
	}

	snprintf(out_config->vmlinux_path, sizeof(out_config->vmlinux_path), 
		 "%s", path);

	return CO_RC(OK);
}

co_rc_t co_load_config_boot_params(co_config_t *out_config, mxml_node_t *node)
{
	char *param_line;
	unsigned long param_line_size_left;
	unsigned long index;
	mxml_node_t *text_node;

	if (node == NULL)
		return CO_RC(ERROR);

	param_line = out_config->boot_parameters_line;
	param_line_size_left = sizeof(out_config->boot_parameters_line);

	co_bzero(param_line, param_line_size_left);

	text_node = node;
	index = 0;

	while (text_node  &&  text_node->type == MXML_TEXT) {
		if (index != 0) {
			int param_size = strlen(param_line);
			param_line += param_size;
			param_line_size_left -= param_size;
		}
						
		snprintf(param_line, 
			 param_line_size_left, 
			 index == 0 ? "%s" : " %s", 
			 text_node->value.text.string);

		index++;
		text_node = text_node->next;
	}

	return CO_RC(OK);
}

co_rc_t co_load_config_initrd(co_config_t *out_config, mxml_element_t *element)
{
	int i;
	char *path = "";

	for (i=0; i < element->num_attrs; i++) {
		mxml_attr_t *attr = &element->attrs[i];
                                                     
		if (strcmp(attr->name, "path") == 0)  
			path = attr->value;
	}

	if (path == NULL) {
		co_debug("config: invalid initrd element: bad path\n");
		return CO_RC(ERROR);
	}

	out_config->initrd_enabled = PTRUE;

	snprintf(out_config->initrd_path, sizeof(out_config->initrd_path),
		 "%s", path);

	return CO_RC(OK);
}


static bool_t char_is_digit(char ch)
{
	return (ch >= '0'  &&  ch <= '9');
}

co_rc_t co_str_to_unsigned_long(const char *text, unsigned long *number_out)
{
	unsigned long number = 0;
	unsigned long last_number = 0;

	if (!char_is_digit(*text))
		return CO_RC(ERROR);

	do {
		last_number = number;
		number *= 10;
		number += (*text - '0');

		if (number < last_number) {
			/* Overflow */
			return CO_RC(ERROR);
		}

	} while (char_is_digit(*++text)) ;

	if (*text == '\0') {
		*number_out = number;
		return CO_RC(OK);
	}

	return CO_RC(ERROR);
}

co_rc_t co_load_config_memory(co_config_t *out_config, mxml_element_t *element)
{
	int i;
	char *element_text = NULL;
	co_rc_t rc;

	for (i=0; i < element->num_attrs; i++) {
		mxml_attr_t *attr = &element->attrs[i];

		if (strcmp(attr->name, "size") == 0)
			element_text = attr->value;
	}
	
	if (element_text == NULL) {
		co_debug("config: invalid memory element: bad memory specification\n");
		return CO_RC(ERROR);
	}

	rc = co_str_to_unsigned_long(element_text, &out_config->ram_size);
	if (!CO_OK(rc))
		co_debug("config: invalid memory element: invalid size format\n");

	return rc;
}

co_rc_t co_load_config_network(co_config_t *out_config, mxml_element_t *element)
{
	int i;
	char *element_text = NULL;
	co_rc_t rc = CO_RC(ERROR);
	co_netdev_desc_t desc = {0, };
	unsigned long index = -1;

	desc.enabled = PFALSE;
	desc.type = CO_NETDEV_TYPE_BRIDGED_PCAP;
	desc.manual_mac_address = PFALSE;

	for (i=0; i < element->num_attrs; i++) {
		mxml_attr_t *attr = &element->attrs[i];

		if (strcmp(attr->name, "mac") == 0) {
			element_text = attr->value;

			rc = co_parse_mac_address(element_text, desc.mac_address);
			if (!CO_OK(rc)) { 
				co_debug("config: invalid network element: invalid mac address specified (use the xx:xx:xx:xx:xx:xx format)\n");
				return rc;
			}

			desc.manual_mac_address = PTRUE;
		} else
		if (strcmp(attr->name, "index") == 0) {
			element_text = attr->value;

			rc = co_str_to_unsigned_long(element_text, &index);
			if (!CO_OK(rc)) {
				co_debug("config: invalid network element: invalid index format\n");
				return CO_RC(ERROR);
			}
			
			if (index < 0  ||  index >= CO_MODULE_MAX_CONET) {
				co_debug("config: invalid network element: invalid index %d\n", (int)index);
				return CO_RC(ERROR);
			}

			desc.enabled = PTRUE;
		} else
		if (strcmp(attr->name, "name") == 0) {
			element_text = attr->value;

			snprintf(desc.desc, sizeof(desc.desc), "%s", element_text);
		} else
		if (strcmp(attr->name, "type") == 0) {
			element_text = attr->value;

			if (strcmp(element_text, "bridged") == 0) {
				desc.type = CO_NETDEV_TYPE_BRIDGED_PCAP;
			} else if (strcmp(element_text, "tap") == 0) {
				desc.type = CO_NETDEV_TYPE_TAP;
			} else {
				co_debug("config: invalid network element: invalid type %s\n", element_text);
				return CO_RC(ERROR);
			}
		}
	}

	if (index == -1) {
		co_debug("config: invalid network element: index not specified\n");
		return CO_RC(ERROR);
	}

	out_config->net_devs[index] = desc;
	
	return rc;
}

co_rc_t co_load_config(char *text, co_config_t *out_config)
{
	mxml_node_t *node, *tree, *walk;
	char *name;
	co_rc_t rc = CO_RC(OK);

	/* Check presence of an UTF-8 BOM marker.
	 * Our XML library doesn't like Byte Order Markers */
	if ( text[0] == '\xEF' && text[1] == '\xBB' && text[2] == '\xBF' )
		text += 3;	// skip it

	out_config->initrd_enabled = PFALSE;

	tree = mxmlLoadString(NULL, text, NULL);
	if (tree == NULL) {
		co_debug("config: error parsing config XML. Please check XML's validity\n");
		return CO_RC(ERROR);
	}

	node = mxmlFindElement(tree, tree, "colinux", NULL, NULL, MXML_DESCEND);

	if (node == NULL) {
		co_debug("config: couldn't find colinux element within XML\n");
		return CO_RC(ERROR);
	}

	for (walk = node->child; walk; walk = walk->next) {
		if (walk->type == MXML_ELEMENT) {
			name = walk->value.element.name;
			rc = CO_RC(OK);

			if (strcmp(name, "block_device") == 0) {
				rc = co_load_config_blockdev(out_config, &walk->value.element);
			} else if (strcmp(name, "bootparams") == 0) {
				rc = co_load_config_boot_params(out_config, walk->child);
			} else if (strcmp(name, "image") == 0) {
				rc = co_load_config_image(out_config, &walk->value.element);
			} else if (strcmp(name, "initrd") == 0) {
				rc = co_load_config_initrd(out_config, &walk->value.element);
			} else if (strcmp(name, "memory") == 0) {
				rc = co_load_config_memory(out_config, &walk->value.element);
			} else if (strcmp(name, "network") == 0) {
				rc = co_load_config_network(out_config, &walk->value.element);
			}

			if (!CO_OK(rc))
				break;
		}
	}

	mxmlRemove(tree);

	if (strcmp(out_config->vmlinux_path, "") == 0)
		snprintf(out_config->vmlinux_path, sizeof(out_config->vmlinux_path), "vmlinux");

	return rc;
}


/************************/

/* 
 * "New" command line configuration gathering scheme. It existed a long time in 
 * User Mode Linux and should be less hussle than handling XML files for the
 * novice users.
 */

static co_rc_t parse_args_config_cobd(co_command_line_params_t cmdline, co_config_t *conf)
{
	bool_t exists;
	char param[0x100];
	co_rc_t rc;

	do {
		int index;
		exists = PFALSE;

		rc = co_cmdline_get_next_equality_int_prefix(cmdline, "cobd", &index, param, 
							     sizeof(param), &exists);
		if (!CO_OK(rc)) 
			return rc;
		
		if (exists) {
			co_block_dev_desc_t *cobd;

			if (index < 0  || index >= CO_MODULE_MAX_COBD) {
				co_terminal_print("invalid cobd index: %d\n", index);
				return CO_RC(ERROR);
			}

			cobd = &conf->block_devs[index];
			cobd->enabled = PTRUE;

			co_snprintf(cobd->pathname, sizeof(cobd->pathname), "%s", param);

			co_canonize_cobd_path(&cobd->pathname);

			co_terminal_print("mapping cobd%d to %s\n", index, cobd->pathname);
		}
	} while (exists);

	return CO_RC(OK);
}

static co_rc_t allocate_by_alias(co_config_t *conf, const char *prefix, const char *suffix, 
				 const char *param)
{
	co_block_dev_desc_t *cobd;
	int i;

	for (i=0; i < CO_MODULE_MAX_COBD; i++) {
		cobd = &conf->block_devs[i];
		
		if (!cobd->enabled)
			break;
	}

	if (i == CO_MODULE_MAX_COBD) {
		co_terminal_print("no available cobd for new alias\n");
		return CO_RC(ERROR);
	}

	cobd->enabled = PTRUE;
	co_snprintf(cobd->pathname, sizeof(cobd->pathname), "%s", param);
	cobd->alias_used = PTRUE;
	snprintf(cobd->alias, sizeof(cobd->alias), "%s%s", prefix, suffix);

	co_canonize_cobd_path(&cobd->pathname);

	co_terminal_print("selected cobd%d for %s%s, mapping to '%s'\n", i, prefix, suffix, cobd->pathname);

	return CO_RC(OK);
}

static co_rc_t parse_args_config_aliases(co_command_line_params_t cmdline, co_config_t *conf)
{
	const char *prefixes[] = {"sd", "hd"};
	const char *prefix;
	bool_t exists;
	char param[0x100];
	co_rc_t rc;
	int i;

	for (i=0; i < sizeof(prefixes)/sizeof(char *); i++) {
		prefix = prefixes[i];
		char suffix[5];
			
		do {
			rc = co_cmdline_get_next_equality(cmdline, prefix, sizeof(suffix)-1, suffix, sizeof(suffix), 
							  param, sizeof(param), &exists);
			if (!CO_OK(rc)) 
				return rc;
			
			if (!exists)
				break;

 			if (!co_strncmp(":cobd", param, 5)) {
				char *index_str = &param[5];
				char *number_parse  = NULL;
				int index;
				co_block_dev_desc_t *cobd;
				
				index = co_strtol(index_str, &number_parse, 10);
				if (number_parse == index_str) {
					co_terminal_print("invalid alias: %s%s=%s\n", prefix, suffix, param);
					return CO_RC(ERROR);
				}

				if (index < 0  || index >= CO_MODULE_MAX_COBD) {
					co_terminal_print("invalid cobd index %d in alias %s%s\n", index, prefix, suffix);
					return CO_RC(ERROR);
				}

				cobd = &conf->block_devs[index];
				if (!cobd->enabled) {
					co_terminal_print("warning alias on disabled cobd%d\n", index);
				}
				
				if (cobd->alias_used) {
					co_terminal_print("error, alias cannot be used twice for cobd%d\n", index);
					return CO_RC(ERROR);
				}

				cobd->alias_used = PTRUE;
				snprintf(cobd->alias, sizeof(cobd->alias), "%s%s", prefix, suffix);

				co_terminal_print("mapping %s%s to param\n", prefix, suffix, &param[1]);
				
			} else {
				rc = allocate_by_alias(conf, prefix, suffix, param);
				if (!CO_OK(rc))
					return rc;
			}
		} while (exists);
	}

	return CO_RC(OK);
}

static bool_t strmatch_identifier(const char *str, const char *identifier, const char **end)
{
	while (*str == *identifier  &&  *str != '\0') {
		str++;
		identifier++;
	}

	if (end)
		*end = str;

	if (*identifier != '\0')
		return PFALSE;

	if (!((*str >= 'a'  &&  *str <= 'z') || (*str >= 'A'  &&  *str <= 'Z'))) {
		if (end) {
			if (*str != '\0')
				(*end)++;
		}
		return PTRUE;
	}

	return PFALSE;
}

static void split_comma_separated(const char *source, char **out_array, int *array_sizes, int array_length)
{
	int index = 0, j;
	
	for (index=0 ; index < array_length ; index++) {
		out_array[index][0] = '\0';
		j = 0;
		while (*source != ','  &&  *source != '\0') {
			if (j == array_sizes[index] - 1) {
				out_array[index][j] = '\0';
				break;
			}
			
			out_array[index][j] = *source;
			source++;
			j++;
		}
		
		if (*source == '\0')
			break;
		
		source++;
	}
}

static co_rc_t parse_args_networking_device_tap(co_config_t *conf, int index, const char *param)
{
	char host_ip[40] = {0, };
	char mac_address[40] = {0, };
	co_rc_t rc;

	char *array[3] = {
		conf->net_devs[index].desc,
		mac_address,
		host_ip,
	};
	int sizes[3] = {
		sizeof(conf->net_devs[index].desc),
		sizeof(mac_address),
		sizeof(host_ip),
	};

	split_comma_separated(param, array, sizes, 3);

	conf->net_devs[index].type = CO_NETDEV_TYPE_TAP;
	conf->net_devs[index].enabled = PTRUE;

	if (strlen(mac_address) > 0) {
		rc = co_parse_mac_address(mac_address, conf->net_devs[index].mac_address);
		if (!CO_OK(rc)) {
			co_terminal_print("error parsing MAC address: %s\n", mac_address);
			return rc;
		}
	}

	co_terminal_print("configured TAP device as eth%d\n", index);

	if (strlen(mac_address) > 0)
		co_terminal_print("MAC address: %s\n", mac_address);
	else
		co_terminal_print("MAC address: auto generated\n");

	if (strlen(host_ip) > 0)
		co_terminal_print("Host IP address: %s (currently ignored)\n", host_ip);

	return CO_RC(OK);
}

static co_rc_t parse_args_networking_device_pcap(co_config_t *conf, int index, const char *param)
{
	char mac_address[40] = {0, };
	co_rc_t rc;

	char *array[3] = {
		conf->net_devs[index].desc,
		mac_address,
	};
	int sizes[3] = {
		sizeof(conf->net_devs[index].desc),
		sizeof(mac_address),
	};

	split_comma_separated(param, array, sizes, 3);

	conf->net_devs[index].type = CO_NETDEV_TYPE_BRIDGED_PCAP;
	conf->net_devs[index].enabled = PTRUE;

	if (strlen(mac_address) > 0) {
		rc = co_parse_mac_address(mac_address, conf->net_devs[index].mac_address);
		if (!CO_OK(rc)) {
			co_terminal_print("error parsing MAC address: %s\n", mac_address);
			return rc;
		}
	}

	if (strlen(conf->net_devs[index].desc) == 0) {
		co_terminal_print("error, the name of the network interface to attach was not specified\n");
		return CO_RC(ERROR);
	}

	co_terminal_print("configured PCAP bridged at '%s' device as eth%d\n", 
			  conf->net_devs[index].desc, index);

	if (strlen(mac_address) > 0)
		co_terminal_print("MAC address: %s\n", mac_address);
	else
		co_terminal_print("MAC address: auto generated\n");

	return CO_RC(OK);
}

static co_rc_t parse_args_networking_device(co_config_t *conf, int index, const char *param)
{
	const char *next = NULL;

	if (strmatch_identifier(param, "tuntap", &next)) {
		return parse_args_networking_device_tap(conf, index, next);
	} else if (strmatch_identifier(param, "pcap-bridge", &next)) {
		return parse_args_networking_device_pcap(conf, index, next);
	} else {
		co_terminal_print("unsupported network transport type: %s\n", param);
		co_terminal_print("supported types are: tuntap, pcap-bridge\n");
		return CO_RC(ERROR);
	}

	return CO_RC(OK);
}

static co_rc_t parse_args_networking(co_command_line_params_t cmdline, co_config_t *conf)
{
	bool_t exists;
	char param[0x100];
	co_rc_t rc;

	do {
		int index;
		exists = PFALSE;

		rc = co_cmdline_get_next_equality_int_prefix(cmdline, "eth", &index, param, 
							     sizeof(param), &exists);
		if (!CO_OK(rc)) 
			return rc;
		
		if (exists) {
			if (index < 0  || index >= CO_MODULE_MAX_CONET) {
				co_terminal_print("invalid network index: %d\n", index);
				return CO_RC(ERROR);
			}

			rc = parse_args_networking_device(conf, index, param);
			if (!CO_OK(rc))
				return rc;
		}
	} while (exists);

	return CO_RC(OK);
}

static co_rc_t parse_config_args(co_command_line_params_t cmdline, co_config_t *conf)
{
	co_rc_t rc;
	bool_t exists;

	rc = co_cmdline_get_next_equality(cmdline, "initrd", 0, NULL, 0, 
					  conf->initrd_path, sizeof(conf->initrd_path),
					  &conf->initrd_enabled);
	if (!CO_OK(rc)) 
		return rc;

	if (conf->initrd_enabled)
		co_terminal_print("using '%s' as initrd image\n", conf->initrd_path);

	rc = co_cmdline_get_next_equality_int_value(cmdline, "mem", (int *)&conf->ram_size, &exists);
	if (!CO_OK(rc)) 
		return rc;

	if (!exists)
		conf->ram_size = 32;
		
	co_terminal_print("configuring %d MB of virtual RAM\n", conf->ram_size);

	rc = parse_args_config_cobd(cmdline, conf);
	if (!CO_OK(rc))
		return rc;

	rc = parse_args_config_aliases(cmdline, conf);
	if (!CO_OK(rc))
		return rc;

	rc = parse_args_networking(cmdline, conf);
	if (!CO_OK(rc))
		return rc;

	return rc;
}

co_rc_t co_parse_config_args(co_command_line_params_t cmdline, co_start_parameters_t *start_parameters)
{
	co_rc_t rc, rc_;
	co_config_t *conf;

	start_parameters->cmdline_config = PFALSE;
	conf = &start_parameters->config;

	rc = co_cmdline_get_next_equality(cmdline, "kernel", 0, NULL, 0, 
					  conf->vmlinux_path, sizeof(conf->vmlinux_path),
					  &start_parameters->cmdline_config);

	if (!CO_OK(rc))
		return rc;

	if (!start_parameters->cmdline_config)
		return CO_RC(OK);

	co_terminal_print("using '%s' as kernel image\n", conf->vmlinux_path);

	rc = parse_config_args(cmdline, conf);
	
	rc_ = co_cmdline_params_format_remaining_parameters(cmdline, conf->boot_parameters_line,
							    sizeof(conf->boot_parameters_line));
	if (!CO_OK(rc_))
		return rc_;
	
	if (CO_OK(rc)) {
		co_terminal_print("kernel boot parameters: '%s'\n", conf->boot_parameters_line);
		co_terminal_print("\n");
		start_parameters->config_specified = PTRUE;
	}

	
	return rc;
}
