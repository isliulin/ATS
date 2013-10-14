/**
 *  dev_prober.cpp
 *
 */

#include "osa.h"
#include "tinyxml.h"
#include "core.h"
#include "config.h"
#include "module/dev_prober.h"


#ifdef __cplusplus
extern "C"
{
#endif

static TiXmlElement *get_module_root(const char *xmlfile, const char *node_name)
{
    osa_assert(xmlfile != NULL);
    osa_assert(node_name != NULL);

    if (osa_file_is_exist(xmlfile) != OSA_TRUE)
    {
        ats_log_error("File not exist: %s!\n", xmlfile);
        return NULL;
    }

    TiXmlDocument   *doc = new TiXmlDocument(xmlfile);
    TiXmlElement    *root = NULL;

    if (doc->LoadFile()!=true)
    {
        ats_log_error("Failed to load xml: %s\n", xmlfile);
        return NULL;
    }

    if ((root = doc->RootElement())==NULL)
    {
        ats_log_error("Failed to get xml root: %s\n", xmlfile);
        return NULL;
    }

    TiXmlElement    *tmp = root->FirstChildElement("module");
    TiXmlAttribute  *attr = NULL;

    while (tmp)
    {
        attr = tmp->FirstAttribute();
        osa_assert(attr != NULL);

        if (!strcmp(attr->Value(), node_name))
        {
            return tmp;
        }
        tmp = tmp->NextSiblingElement("module");
    }

    return NULL;
}

void ats_devpb_probe()
{
    ats_bus_t   *dev_bus = ats_bus_find("dev_bus");
    if (!dev_bus)
    {
        ats_log_error("No device bus find!\n");
    }

    TiXmlElement *mroot = get_module_root(ATS_CONFIG_FILE, "device");
    osa_assert(mroot != NULL);


    TiXmlElement    *dev_node = NULL;
    TiXmlElement    *info_node = NULL;
    TiXmlElement    *tmp = NULL;

    const char 		*dev_name = NULL;
    ats_devtype_t   dev_type = DT_DUMMY;
    const char 		*addr = NULL;
    const char 		*user = NULL;
    const char 		*password = NULL;
    const char		*drv_file = NULL;
    const char		*sdk_plugin = NULL;

    dev_node = mroot->FirstChildElement("device");
    osa_assert(dev_node != NULL);

    while (dev_node)
    {
        info_node = dev_node->FirstChildElement("info");
        osa_assert(info_node != NULL);

        dev_name = dev_node->Attribute("name");
        osa_assert(dev_name != NULL);

        dev_type = (ats_devtype_t)atoi(dev_node->Attribute("type"));
        osa_assert(dev_type > 0);

        tmp = info_node->FirstChildElement("addr");
        osa_assert(tmp != NULL);
        addr = (tmp->FirstChild()) ? tmp->FirstChild()->Value(): NULL;

        tmp = info_node->FirstChildElement("user");
        osa_assert(tmp != NULL);
        user = (tmp->FirstChild()) ? tmp->FirstChild()->Value(): NULL;

        tmp = info_node->FirstChildElement("password");
        osa_assert(tmp != NULL);
        password = (tmp->FirstChild()) ? tmp->FirstChild()->Value(): NULL;

        tmp = dev_node->FirstChildElement("drv_file");
        osa_assert(tmp != NULL);
        drv_file = (tmp->FirstChild()) ? tmp->FirstChild()->Value(): NULL;

        tmp = dev_node->FirstChildElement("sdk_plugin");
        osa_assert(tmp != NULL);
        sdk_plugin = (tmp->FirstChild()) ? tmp->FirstChild()->Value(): NULL;

        ats_device_t *new_dev = ats_device_new(dev_name, dev_type);
        if (!new_dev)
        {
            ats_log_error("Failed to new device object!\n");
            continue;
        }

        ats_device_setinfo(new_dev, addr, user, password);

        ats_devpb_t *dp = NULL;
        osa_uint32_t i;

        for (i=0; i<g_dpnum; i++)
        {
            dp = g_dptable[i];
            if (ats_devpb_is_support(dp, new_dev->name) != OSA_TRUE)
            {
                continue;
            }

            if (dp->dev_is_ok && dp->dev_is_ok(new_dev) == OSA_TRUE)
            {
                if (ats_device_find(dev_bus, new_dev->name) == NULL)
                {
                    ats_sdk_t *sdk = ats_sdk_plugin_load(sdk_plugin);
                    if (!sdk)
                    {
                        ats_log_warn("Failed to load sdk for device : %s\n", new_dev->name);
                    }
                    else
                    {
                        ats_log_info("Succeed to load sdk for device : %s\n", new_dev->name);
                    }

                    ats_tdrv_t *drv = ats_tdrv_load(drv_file);
                    if (!drv)
                    {
                        ats_log_warn("Failed to load test driver for device : %s\n", new_dev->name);
                    }
                    else
                    {
                        ats_log_info("Succeed to load test driver for device: %s\n", new_dev->name);
                    }

                    new_dev->sdk = sdk;
                    new_dev->drv = drv;

                    ats_device_register(dev_bus, new_dev);
                }
            }
            else
            {
                ats_device_unregister(dev_bus, new_dev->name);
            }

            break;
        }

        dev_node = dev_node->NextSiblingElement("device");
    }
}

osa_bool_t ats_devpb_is_support(ats_devpb_t *dp, const osa_char_t *dev_name)
{
    osa_assert(dev_name != NULL);

    if (!dp->dev_support)
    {
        return OSA_FALSE;
    }

    osa_char_t *devs = (osa_char_t *)osa_mem_alloc(strlen(dp->dev_support));
    strncpy(devs, dp->dev_support, strlen(dp->dev_support));

    char *p = strtok(devs, ",");
    while (p != NULL)
    {
        if (!strcmp(p, dev_name))
        {
            osa_mem_free(devs);
            return OSA_TRUE;
        }
        p = strtok(NULL, ",");
    }
    osa_mem_free(devs);
    return OSA_FALSE;
}

#ifdef __cplusplus
}
#endif
