vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/libevent/libevent/releases/download/release-2.1.13-stable/libevent-2.1.13-stable.tar.gz"
    FILENAME "libevent-2.1.13-stable.tar.gz"
    SHA512 5c7a1b9f48087258f49653ecb4ce24d7015a9ef4015ad6ffa910a101d5bd37c657c26fc948eaae30c4abeb54d1cda33290878161bd73bb50e4e85f7dc667c36d
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}")
vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    INVERTED_FEATURES
        openssl EVENT__DISABLE_OPENSSL
        thread EVENT__DISABLE_THREAD_SUPPORT
)
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    set(LIBRARY_TYPE STATIC)
else()
    set(LIBRARY_TYPE SHARED)
endif()
if(VCPKG_CRT_LINKAGE STREQUAL "static")
    set(STATIC_RUNTIME ON)
else()
    set(STATIC_RUNTIME OFF)
endif()
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${FEATURE_OPTIONS}
        -DEVENT__LIBRARY_TYPE=${LIBRARY_TYPE}
        -DEVENT__MSVC_STATIC_RUNTIME=${STATIC_RUNTIME}
        -DEVENT__DISABLE_BENCHMARK=ON
        -DEVENT__DISABLE_TESTS=ON
        -DEVENT__DISABLE_REGRESS=ON
        -DEVENT__DISABLE_SAMPLES=ON
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/libevent)
vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/share/libevent/LibeventConfig.cmake"
    "${SOURCE_PATH}/include;${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/include"
    "\${_INSTALL_PREFIX}/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/tools/libevent")
file(RENAME "${CURRENT_PACKAGES_DIR}/bin/event_rpcgen.py"
            "${CURRENT_PACKAGES_DIR}/tools/libevent/event_rpcgen.py")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
