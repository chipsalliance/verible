STATIC_EXECUTABLES_FEATURE = select({
    "//bazel:static_linked_executables": ["fully_static_link"],
    "//conditions:default": [],
})
