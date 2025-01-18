def get_version_define_from_module():
    module_version = native.module_version()
    if module_version:
        return ['-DVERIBLE_MODULE_VERSION=\\"{0}\\"'.format(module_version)]
    else:
        return []
