# Copyright (c) 2014-2024, The Monero Project
# 
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
# 
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers


# This is broken up into two parts: first we check for miniupnp, compile it if we can't
# find it, and thereafter we check for libunbound, and compile it if we can't find it.
# We always compile if we are building statically to reduce static dependency issues...
# ...except for FreeBSD, because FreeBSD is a special case that doesn't play well with
# others.

find_package(Miniupnpc REQUIRED)

message(STATUS "Using in-tree miniupnpc")
set(UPNPC_NO_INSTALL TRUE CACHE BOOL "Disable miniupnp installation" FORCE)
set(UPNPC_BUILD_SHARED OFF CACHE BOOL "Disable building shared library" FORCE)
add_subdirectory(miniupnp/miniupnpc)
set_property(TARGET libminiupnpc-static PROPERTY FOLDER "external")
set_property(TARGET libminiupnpc-static PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET libminiupnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-undef -Wno-unused-result -Wno-unused-value")
if(CMAKE_SYSTEM_NAME MATCHES "NetBSD")
	set_property(TARGET libminiupnpc-static APPEND_STRING PROPERTY COMPILE_FLAGS " -D_NETBSD_SOURCE")
endif()

set(UPNP_LIBRARIES "libminiupnpc-static" PARENT_SCOPE)

find_package(Unbound)

if(NOT UNBOUND_INCLUDE_DIR)
  die("Could not find libunbound")
else()
  message(STATUS "Found libunbound include (unbound.h) in ${UNBOUND_INCLUDE_DIR}")
  if(UNBOUND_LIBRARIES)
    message(STATUS "Found libunbound library")
  else()
    die("Found libunbound includes, but could not find libunbound library. Please make sure you have installed libunbound or libunbound-dev or the equivalent")
  endif()
endif()

add_subdirectory(db_drivers)
add_subdirectory(easylogging++)
add_subdirectory(qrcodegen)
add_subdirectory(randomx EXCLUDE_FROM_ALL)

# Phase 5: Post-Quantum Cryptography (Open Quantum Safe / liboqs 0.15.x).
# Provides ML-DSA-65 (FIPS 204 signatures) and ML-KEM-768 (FIPS 203 KEM).
#
# liboqs is built standalone, static, OpenSSL-backed, library-only:
#   cmake -S external/liboqs -B external/liboqs/build \
#         -DBUILD_SHARED_LIBS=OFF -DOQS_USE_OPENSSL=ON -DOQS_BUILD_ONLY_LIB=ON
#   cmake --build external/liboqs/build -j$(nproc)
#
# We expose it as the IMPORTED `oqs` target rather than add_subdirectory():
# liboqs' in-tree CMake defines a helper target named `common`, which collides
# with HIDERING's own `common` target (CMP0002). An imported target pulls in
# only the prebuilt archive + headers, so there is no name clash and liboqs is
# not rebuilt as part of the daemon build. See src/crypto/pqc_test.cpp.
set(LIBOQS_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/liboqs/build")
if(EXISTS "${LIBOQS_BUILD_DIR}/lib/liboqs.a")
  add_library(oqs STATIC IMPORTED GLOBAL)
  set_target_properties(oqs PROPERTIES
    IMPORTED_LOCATION "${LIBOQS_BUILD_DIR}/lib/liboqs.a"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBOQS_BUILD_DIR}/include"
    INTERFACE_LINK_LIBRARIES "${OPENSSL_CRYPTO_LIBRARY}")
  message(STATUS "Phase 5 PQC: liboqs enabled (imported target: oqs)")
else()
  message(STATUS "Phase 5 PQC: liboqs not built (run the external/liboqs build); oqs target unavailable")
endif()
